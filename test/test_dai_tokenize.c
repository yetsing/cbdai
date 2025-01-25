//
// Created by  on 2024/5/29.
//

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "munit/munit.h"

#include "dai_codecs.h"
#include "dai_tokenize.h"
#include "dai_utils.h"

// #region tokenize 辅助函数

static DaiTokenList*
dai_tokenize_file(const char* filename) {
    DaiTokenList* list = DaiTokenList_New();
    char* s            = dai_string_from_file(filename);
    munit_assert_not_null(s);
    DaiSyntaxError* err = dai_tokenize_string(s, list);
    if (err) {
        DaiSyntaxError_setFilename(err, filename);
        DaiSyntaxError_pprint(err, s);
    }
    munit_assert_null(err);
    free(s);
    return list;
}

// #endregion

static MunitResult
test_next_token(__attribute__((unused)) const MunitParameter params[],
                __attribute__((unused)) void* user_data) {
    const char* input      = "=+(){},;\n"
                             "var five = 5;\n"
                             "var ten = 10;\n"
                             "\n"
                             "var add = fn(x, y) {\n"
                             "   x + y;\n"
                             "};\n"
                             "\n"
                             "var result = add(five, ten);\n"
                             "!-/*5;\n"
                             "5 < 10 > 5;\n"
                             "\n"
                             "if (5 < 10) {\n"
                             "     return true;\n"
                             "} else {\n"
                             "     return false;\n"
                             "}\n"
                             "\n"
                             "10 == 10;\n"
                             "10 != 9; # 123\n"
                             "//456\n"
                             "#中文注释123\n"
                             "var 中文 = 123;\n"
                             "\"\" \"a\" \"abcd\" \"中文\";\n"
                             "'' 'a' 'abcd' '中文';\n"
                             "'  a  b  '\n"
                             "`  a  b  \n`\n"
                             "class Foo {};\n"
                             "self.a = 3;\n"
                             "super.b elif nil\n"
                             "0 "
                             "3.4 "
                             "0.1 "
                             "0.0 "
                             "1.0 "
                             "3.1416 "
                             "1.234E+10 "
                             "1.234E-10 % \n"
                             "== != < > <= >= and or not\n"
                             "& | ~ << >> ^\n"
                             "[]: += -= *= /= \n";
    const DaiToken tests[] = {
        {
            DaiTokenType_assign,
            "=",
            1,
            1,
            1,
            2,
        },
        {
            DaiTokenType_plus,
            "+",
            1,
            2,
            1,
            3,
        },
        {
            DaiTokenType_lparen,
            "(",
            1,
            3,
            1,
            4,
        },
        {
            DaiTokenType_rparen,
            ")",
            1,
            4,
            1,
            5,
        },
        {
            DaiTokenType_lbrace,
            "{",
            1,
            5,
            1,
            6,
        },
        {
            DaiTokenType_rbrace,
            "}",
            1,
            6,
        },
        {DaiTokenType_comma, ",", 1, 7},
        {DaiTokenType_semicolon, ";", 1, 8},

        {DaiTokenType_var, "var", 2, 1},
        {DaiTokenType_ident, "five", 2, 5},
        {DaiTokenType_assign, "=", 2, 10},
        {DaiTokenType_int, "5", 2, 12},
        {DaiTokenType_semicolon, ";", 2, 13},

        {DaiTokenType_var, "var", 3, 1},
        {DaiTokenType_ident, "ten", 3, 5},
        {DaiTokenType_assign, "=", 3, 9},
        {DaiTokenType_int, "10", 3, 11},
        {DaiTokenType_semicolon, ";", 3, 13},

        {DaiTokenType_var, "var", 5, 1},
        {DaiTokenType_ident, "add", 5, 5},
        {DaiTokenType_assign, "=", 5, 9},
        {DaiTokenType_function, "fn", 5, 11},
        {DaiTokenType_lparen, "(", 5, 13},
        {DaiTokenType_ident, "x", 5, 14},
        {DaiTokenType_comma, ",", 5, 15},
        {DaiTokenType_ident, "y", 5, 17},
        {DaiTokenType_rparen, ")", 5, 18},
        {DaiTokenType_lbrace, "{", 5, 20},

        {DaiTokenType_ident, "x", 6, 4},
        {DaiTokenType_plus, "+", 6, 6},
        {DaiTokenType_ident, "y", 6, 8},
        {DaiTokenType_semicolon, ";", 6, 9},

        {DaiTokenType_rbrace, "}", 7, 1},
        {DaiTokenType_semicolon, ";", 7, 2},

        {DaiTokenType_var, "var", 9, 1},
        {DaiTokenType_ident, "result", 9, 5},
        {DaiTokenType_assign, "=", 9, 12},
        {DaiTokenType_ident, "add", 9, 14},
        {DaiTokenType_lparen, "(", 9, 17},
        {DaiTokenType_ident, "five", 9, 18},
        {DaiTokenType_comma, ",", 9, 22},
        {DaiTokenType_ident, "ten", 9, 24},
        {DaiTokenType_rparen, ")", 9, 27},
        {DaiTokenType_semicolon, ";", 9, 28},

        {DaiTokenType_bang, "!", 10, 1},
        {DaiTokenType_minus, "-", 10, 2},
        {DaiTokenType_slash, "/", 10, 3},
        {DaiTokenType_asterisk, "*", 10, 4},
        {DaiTokenType_int, "5", 10, 5},
        {DaiTokenType_semicolon, ";", 10, 6},

        {DaiTokenType_int, "5", 11, 1},
        {DaiTokenType_lt, "<", 11, 3},
        {DaiTokenType_int, "10", 11, 5},
        {DaiTokenType_gt, ">", 11, 8},
        {DaiTokenType_int, "5", 11, 10},
        {DaiTokenType_semicolon, ";", 11, 11},

        {DaiTokenType_if, "if", 13, 1},
        {DaiTokenType_lparen, "(", 13, 4},
        {DaiTokenType_int, "5", 13, 5},
        {DaiTokenType_lt, "<", 13, 7},
        {DaiTokenType_int, "10", 13, 9},
        {DaiTokenType_rparen, ")", 13, 11},
        {DaiTokenType_lbrace, "{", 13, 13},

        {DaiTokenType_return, "return", 14, 6},
        {DaiTokenType_true, "true", 14, 13},
        {DaiTokenType_semicolon, ";", 14, 17},

        {DaiTokenType_rbrace, "}", 15, 1},
        {DaiTokenType_else, "else", 15, 3},
        {DaiTokenType_lbrace, "{", 15, 8},

        {DaiTokenType_return, "return", 16, 6},
        {DaiTokenType_false, "false", 16, 13},
        {DaiTokenType_semicolon, ";", 16, 18},

        {DaiTokenType_rbrace, "}", 17, 1},

        {DaiTokenType_int, "10", 19, 1},
        {DaiTokenType_eq, "==", 19, 4},
        {DaiTokenType_int, "10", 19, 7},
        {DaiTokenType_semicolon, ";", 19, 9},

        {DaiTokenType_int, "10", 20, 1},
        {DaiTokenType_not_eq, "!=", 20, 4},
        {DaiTokenType_int, "9", 20, 7},
        {DaiTokenType_semicolon, ";", 20, 8},
        {DaiTokenType_comment, "# 123", 20, 10},

        {DaiTokenType_comment, "//456", 21, 1},

        {DaiTokenType_comment, "#中文注释123", 22, 1},

        {DaiTokenType_var, "var", 23, 1},
        {DaiTokenType_ident, "中文", 23, 5},
        {DaiTokenType_assign, "=", 23, 8},
        {DaiTokenType_int, "123", 23, 10},
        {DaiTokenType_semicolon, ";", 23, 13},

        {DaiTokenType_str, "\"\"", 24, 1},
        {DaiTokenType_str, "\"a\"", 24, 4},
        {DaiTokenType_str, "\"abcd\"", 24, 8},
        {DaiTokenType_str, "\"中文\"", 24, 15},
        {DaiTokenType_semicolon, ";", 24, 19},

        {DaiTokenType_str, "''", 25, 1},
        {DaiTokenType_str, "'a'", 25, 4},
        {DaiTokenType_str, "'abcd'", 25, 8},
        {DaiTokenType_str, "'中文'", 25, 15},
        {DaiTokenType_semicolon, ";", 25, 19},

        {DaiTokenType_str, "'  a  b  '", 26, 1},

        {DaiTokenType_str, "`  a  b  \n`", 27, 1, 28, 2},

        {DaiTokenType_class, "class", 29, 1, 29, 6},
        {DaiTokenType_ident, "Foo", 29, 7, 29, 10},
        {DaiTokenType_lbrace, "{", 29, 11, 29, 12},
        {DaiTokenType_rbrace, "}", 29, 12, 29, 13},
        {DaiTokenType_semicolon, ";", 29, 13, 29, 14},

        {DaiTokenType_self, "self", 30, 1, 30, 5},
        {DaiTokenType_dot, ".", 30, 5, 30, 6},
        {DaiTokenType_ident, "a", 30, 6, 30, 7},
        {DaiTokenType_assign, "=", 30, 8, 30, 9},
        {DaiTokenType_int, "3", 30, 10, 30, 11},
        {DaiTokenType_semicolon, ";", 30, 11, 30, 12},

        {DaiTokenType_super, "super", 31, 1, 31, 6},
        {DaiTokenType_dot, ".", 31, 6, 31, 7},
        {DaiTokenType_ident, "b", 31, 7, 31, 8},
        {DaiTokenType_elif, "elif", 31, 9, 31, 13},
        {DaiTokenType_nil, "nil", 31, 14, 31, 17},

        {DaiTokenType_int, "0", 32, 1, 32, 2},
        {DaiTokenType_float, "3.4", 32, 3, 32, 6},
        {DaiTokenType_float, "0.1", 32, 7, 32, 10},
        {DaiTokenType_float, "0.0", 32, 11, 32, 14},
        {DaiTokenType_float, "1.0", 32, 15, 32, 18},
        {DaiTokenType_float, "3.1416", 32, 19, 32, 25},
        {DaiTokenType_float, "1.234E+10", 32, 26, 32, 35},
        {DaiTokenType_float, "1.234E-10", 32, 36, 32, 45},
        {DaiTokenType_percent, "%", 32, 46, 32, 47},

        {DaiTokenType_eq, "==", 33, 1, 33, 3},
        {DaiTokenType_not_eq, "!=", 33, 4, 33, 6},
        {DaiTokenType_lt, "<", 33, 7, 33, 8},
        {DaiTokenType_gt, ">", 33, 9, 33, 10},
        {DaiTokenType_lte, "<=", 33, 11, 33, 13},
        {DaiTokenType_gte, ">=", 33, 14, 33, 16},
        {DaiTokenType_and, "and", 33, 17, 33, 20},
        {DaiTokenType_or, "or", 33, 21, 33, 23},
        {DaiTokenType_not, "not", 33, 24, 33, 27},

        {DaiTokenType_bitwise_and, "&", 34, 1, 34, 2},
        {DaiTokenType_bitwise_or, "|", 34, 3, 34, 4},
        {DaiTokenType_bitwise_not, "~", 34, 5, 34, 6},
        {DaiTokenType_left_shift, "<<", 34, 7, 34, 9},
        {DaiTokenType_right_shift, ">>", 34, 10, 34, 12},
        {DaiTokenType_bitwise_xor, "^", 34, 13, 34, 14},

        {DaiTokenType_lbracket, "[", 35, 1, 35, 2},
        {DaiTokenType_rbracket, "]", 35, 2, 35, 3},
        {DaiTokenType_colon, ":", 35, 3, 35, 4},
        {DaiTokenType_add_assign, "+=", 35, 5, 35, 7},
        {DaiTokenType_sub_assign, "-=", 35, 8, 35, 10},
        {DaiTokenType_mul_assign, "*=", 35, 11, 35, 13},
        {DaiTokenType_div_assign, "/=", 35, 14, 35, 16},

        {DaiTokenType_eof, "", 36, 1},
    };

    DaiTokenList list;
    DaiTokenList_init(&list);
    DaiSyntaxError* err = dai_tokenize_string(input, &list);
    if (err) {
        DaiSyntaxError_setFilename(err, "<test>");
        DaiSyntaxError_pprint(err, input);
    }
    munit_assert_null(err);
    for (int i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        const DaiToken expect = tests[i];
        const DaiToken* tok   = DaiTokenList_next(&list);
        munit_assert_string_equal(DaiTokenType_string(expect.type), DaiTokenType_string(tok->type));
        munit_assert_int(expect.type, ==, tok->type);
        munit_assert_string_equal(expect.literal, tok->literal);
        munit_assert_int(expect.start_line, ==, tok->start_line);
        munit_assert_int(expect.start_column, ==, tok->start_column);
        if (expect.end_line == 0) {
            munit_assert_int(tok->start_line, ==, tok->end_line);
            munit_assert_int(
                tok->start_column + dai_utf8_strlen(tok->literal), ==, tok->end_column);

        } else {
            munit_assert_int(expect.end_line, ==, tok->end_line);
            munit_assert_int(expect.end_column, ==, tok->end_column);
        }
    }
    DaiTokenList_reset(&list);
    return MUNIT_OK;
}

static MunitResult
test_tokenize_file(__attribute__((unused)) const MunitParameter params[],
                   __attribute__((unused)) void* user_data) {
    const char* filename = "test_tokenize_file.dai";
    unlink(filename);
    FILE* fp = fopen(filename, "w");
    munit_assert_not_null(fp);

    const char* input = "\nvar _Fiv = 5;=\t\r";
    const size_t wn   = fwrite(input, strlen(input), 1, fp);
    munit_assert_size(1, ==, wn);
    fclose(fp);
    DaiToken tests[] = {
        {DaiTokenType_var, "var", 2, 1},
        {DaiTokenType_ident, "_Fiv", 2, 5},
        {DaiTokenType_assign, "=", 2, 10},
        {DaiTokenType_int, "5", 2, 12},
        {DaiTokenType_semicolon, ";", 2, 13},
        {DaiTokenType_assign, "=", 2, 14},
        {DaiTokenType_eof, "", 2, 17},
    };
    DaiTokenList* list = dai_tokenize_file(filename);
    munit_assert_size(sizeof(tests) / sizeof(tests[0]), ==, DaiTokenList_length(list));
    for (int i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        DaiToken expect = tests[i];
        DaiToken* tok   = DaiTokenList_next(list);
        munit_assert_int(expect.type, ==, tok->type);
        munit_assert_string_equal(expect.literal, tok->literal);
        munit_assert_int(expect.start_line, ==, tok->start_line);
        munit_assert_int(expect.start_column, ==, tok->start_column);
        munit_assert_int(tok->start_line, ==, tok->end_line);
        munit_assert_int(tok->start_column + dai_utf8_strlen(tok->literal), ==, tok->end_column);
    }
    DaiTokenList_free(list);
    unlink(filename);

    return MUNIT_OK;
}


static MunitResult
test_tokenize_integer(__attribute__((unused)) const MunitParameter params[],
                      __attribute__((unused)) void* user_data) {
    const char* tests[] = {
        "0",

        "1",
        "12",
        "123",
        "12___3",
        "1234567890",
        "1_2_3_4_5_6_7_8",
        "1_____________2",

        "0b0",
        "0b0101010",
        "0b_0_1_01010",
        "0b____0___1_01010",
        "0B0",
        "0B0101010",
        "0B_0_1_01010",
        "0B____0___1_01010",

        "0o0",
        "0o01234567",
        "0o_0_1_2_3_4_5_6_7",
        "0o__________0_1_2_3_4_5_6___7",
        "0O0",
        "0O01234567",
        "0O_0_1_2_3_4_5_6_7",
        "0O__________0_1_2_3_4_5_6_7",

        "0x0",
        "0x0123456789abcdefABCDEF",
        "0x_0_1_23456789_abcdef_ABCDEF",
        "0x___0_1_23456____789_ab_c__def_ABCDEF",
        "0X0",
        "0X0123456789abcdefABCDEF",
        "0X_0_1_23456789_abcdef_ABCDEF",
        "0X___0_1_23456____789_ab_c__def_ABCDEF",
    };
    for (int i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        const char* input = tests[i];
        DaiTokenList list;
        DaiTokenList_init(&list);
        DaiSyntaxError* err = dai_tokenize_string(input, &list);
        if (err) {
            printf("input: \"%s\"\n", input);
            DaiSyntaxError_setFilename(err, "<test>");
            DaiSyntaxError_pprint(err, input);
        }
        if (2 != DaiTokenList_length(&list)) {
            printf("input: \"%s\"\n", input);
            for (int j = 0; j < DaiTokenList_length(&list); j++) {
                const DaiToken* tok = DaiTokenList_next(&list);
                printf("token: \"%s\"\n", tok->literal);
            }
        }
        munit_assert_int(2, ==, DaiTokenList_length(&list));
        {
            DaiToken* tok = DaiTokenList_next(&list);
            munit_assert_int(DaiTokenType_int, ==, tok->type);
            munit_assert_string_equal(input, tok->literal);
        }
        {
            DaiToken* tok = DaiTokenList_next(&list);
            munit_assert_int(DaiTokenType_eof, ==, tok->type);
        }
        munit_assert_null(err);
        DaiTokenList_reset(&list);
    }
    return MUNIT_OK;
}

static MunitResult
test_tokenize_float(__attribute__((unused)) const MunitParameter params[],
                    __attribute__((unused)) void* user_data) {
    const char* tests[] = {
        "0.0",
        "0.00",
        "0.0____0",
        "0.0123456789",
        "0.0_1_2_3_4_5_6_7_8_9",
        "0.0_1_____2_____3_____4_5_67_8_____9",

        "1.0",
        "1.00",
        "1.0____0",
        "1.0123456789",
        "1.0_1_2_3_4_5_6_7_8_9",
        "1.0_1_____2_____3_____4_5_67_8_____9",

        "10.0",
        "20.00",
        "30.0____0",
        "40.0123456789",
        "50.0_1_2_3_4_5_6_7_8_9",
        "60.0_1_____2_____3_____4_5_67_8_____9",
        "70.0_1_____2_____3_____4_5_67_8_____9",
        "80.0_1_____2_____3_____4_5_67_8_____9",
        "90.0_1_____2_____3_____4_5_67_8_____9",

        "100.0",
        "1_2_3.4_5_6_7_8",

        "1.234E+10",
        "1.234E-10",

        "1.0000000000000002",
        "4.9406564584124654e-324",
        "2.2250738585072009e-308",
        "2.2250738585072014e-308",
        "1.7976931348623157e+308",
    };
    for (int i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        const char* input = tests[i];
        DaiTokenList list;
        DaiTokenList_init(&list);
        DaiSyntaxError* err = dai_tokenize_string(input, &list);
        if (err) {
            printf("input: \"%s\"\n", input);
            DaiSyntaxError_setFilename(err, "<test>");
            DaiSyntaxError_pprint(err, input);
        }
        if (2 != DaiTokenList_length(&list)) {
            printf("input: \"%s\"\n", input);
            for (int j = 0; j < DaiTokenList_length(&list); j++) {
                const DaiToken* tok = DaiTokenList_next(&list);
                printf("token: \"%s\"\n", tok->literal);
            }
        }
        munit_assert_int(2, ==, DaiTokenList_length(&list));
        {
            DaiToken* tok = DaiTokenList_next(&list);
            munit_assert_int(DaiTokenType_float, ==, tok->type);
            munit_assert_string_equal(input, tok->literal);
        }
        {
            DaiToken* tok = DaiTokenList_next(&list);
            munit_assert_int(DaiTokenType_eof, ==, tok->type);
        }
        munit_assert_null(err);
        DaiTokenList_reset(&list);
    }
    return MUNIT_OK;
}



static MunitResult
test_illegal_token(__attribute__((unused)) const MunitParameter params[],
                   __attribute__((unused)) void* user_data) {
    struct {
        const char* input;
        const char* error_message;
    } tests[] = {
        {"$$$", "SyntaxError: illegal character '$' in <stdin>:1:1"},
        {"\xbf", "SyntaxError: invalid utf8 encoding character in <stdin>:1:1"},
        {"'a\n'", "SyntaxError: unclosed string literal in <stdin>:1:3"},
        {"01",
         "SyntaxError: leading zeros in decimal integer literals are not permitted in <stdin>:1:1"},
        {".01",
         "SyntaxError: leading zeros in decimal integer literals are not permitted in <stdin>:1:2"},
        {"0b2", "SyntaxError: invalid number in <stdin>:1:1"},
        {"0B2", "SyntaxError: invalid number in <stdin>:1:1"},
        {"0o8", "SyntaxError: invalid number in <stdin>:1:1"},
        {"0O8", "SyntaxError: invalid number in <stdin>:1:1"},
        {"0xg", "SyntaxError: invalid number in <stdin>:1:1"},
        {"0Xg", "SyntaxError: invalid number in <stdin>:1:1"},
        {"1_", "SyntaxError: invalid number in <stdin>:1:1"},
        {"0.", "SyntaxError: invalid number in <stdin>:1:1"},
        {"1.", "SyntaxError: invalid number in <stdin>:1:1"},
        {"0.1E", "SyntaxError: invalid number in <stdin>:1:1"},
        {"0.1E+", "SyntaxError: invalid number in <stdin>:1:1"},
        {"0.1E-", "SyntaxError: invalid number in <stdin>:1:1"},
        {"，", "SyntaxError: illegal character '，' in <stdin>:1:1"},
        {"？", "SyntaxError: illegal character '？' in <stdin>:1:1"},
    };
    for (int i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        const char* input = tests[i].input;
        DaiTokenList list;
        DaiTokenList_init(&list);
        DaiSyntaxError* err = dai_tokenize_string(input, &list);
        DaiSyntaxError_setFilename(err, "<stdin>");
        munit_assert_not_null(err);
        {
            char* s = DaiSyntaxError_string(err);
            munit_assert_string_equal(s, tests[i].error_message);
            free(s);
        }
        DaiTokenList_reset(&list);
        DaiSyntaxError_free(err);
    }
    return MUNIT_OK;
}

bool
assert_string_starts_with(const char* s, const char* prefix) {
    const size_t prefix_length = strlen(prefix);
    const size_t slen          = strlen(s);
    if (slen < prefix_length) {
        return false;
    }
    for (size_t i = 0; i < prefix_length; ++i) {
        if (s[i] != prefix[i]) {
            return false;
        }
    }
    return true;
}

static MunitResult
test_token_type_string(__attribute__((unused)) const MunitParameter params[],
                       __attribute__((unused)) void* user_data) {
    for (int i = 0; i < DaiTokenType_end; ++i) {
        const char* s = DaiTokenType_string(i);
        munit_assert_true(assert_string_starts_with(s, "DaiTokenType_"));
        munit_assert_string_not_equal(s, "DaiTokenType_end");
    }
    const char* s = DaiTokenType_string(DaiTokenType_end);
    munit_assert_string_equal(s, "DaiTokenType_end");
    return MUNIT_OK;
}
MunitTest tokenize_suite_tests[] = {
    {(char*)"/test_next_token", test_next_token, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/test_tokenize_file", test_tokenize_file, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/test_tokenize_integer",
     test_tokenize_integer,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/test_tokenize_float", test_tokenize_float, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/test_illegal_token", test_illegal_token, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/test_token_type_string",
     test_token_type_string,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};
