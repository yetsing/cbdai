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

static DaiTokenList *dai_tokenize_file(const char *filename) {
  DaiTokenList *list = DaiTokenList_New();
  char *s = dai_string_from_file(filename);
  munit_assert_not_null(s);
  DaiSyntaxError *err = dai_tokenize_string(s, list);
  if (err) {
    DaiSyntaxError_setFilename(err, filename);
    DaiSyntaxError_pprint(err, s);
  }
  munit_assert_null(err);
  free(s);
  return list;
}

// #endregion

static MunitResult test_next_token(__attribute__((unused))
                                   const MunitParameter params[],
                                   __attribute__((unused)) void *user_data) {
  const char *input = "=+(){},;\n"
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
                      "super.b\n";

  DaiToken tests[] = {
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

      {DaiTokenType_eof, "", 32, 1},
  };

  DaiTokenList list;
  DaiTokenList_init(&list);
  DaiSyntaxError *err = dai_tokenize_string(input, &list);
  if (err) {
    DaiSyntaxError_setFilename(err, "<test>");
    DaiSyntaxError_pprint(err, input);
  }
  munit_assert_null(err);
  for (int i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
    DaiToken expect = tests[i];
    DaiToken *tok = DaiTokenList_next(&list);
    munit_assert_int(expect.type, ==, tok->type);
    munit_assert_string_equal(expect.literal, tok->literal);
    munit_assert_int(expect.start_line, ==, tok->start_line);
    munit_assert_int(expect.start_column, ==, tok->start_column);
    if (expect.end_line == 0) {
    munit_assert_int(tok->start_line, ==, tok->end_line);
    munit_assert_int(tok->start_column + dai_utf8_strlen(tok->literal), ==,
                     tok->end_column);
      
    } else {
      munit_assert_int(expect.end_line, ==, tok->end_line);
      munit_assert_int(expect.end_column, ==, tok->end_column);
    }
  }
  DaiTokenList_reset(&list);
  return MUNIT_OK;
}

static MunitResult test_tokenize_file(__attribute__((unused))
                                      const MunitParameter params[],
                                      __attribute__((unused)) void *user_data) {
  const char *filename = "test_tokenize_file.dai";
  unlink(filename);
  FILE *fp = fopen(filename, "w");
  munit_assert_not_null(fp);

  const char *input = "\nvar _Fiv = 5;=\t\r";
  size_t wn = fwrite(input, strlen(input), 1, fp);
  munit_assert_size(1, ==, wn);
  fclose(fp);
  DaiToken tests[] = {
      {DaiTokenType_var, "var", 2, 1},      {DaiTokenType_ident, "_Fiv", 2, 5},
      {DaiTokenType_assign, "=", 2, 10},    {DaiTokenType_int, "5", 2, 12},
      {DaiTokenType_semicolon, ";", 2, 13}, {DaiTokenType_assign, "=", 2, 14},
      {DaiTokenType_eof, "", 2, 17},
  };
  DaiTokenList *list = dai_tokenize_file(filename);
  munit_assert_size(sizeof(tests) / sizeof(tests[0]), ==,
                    DaiTokenList_length(list));
  for (int i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
    DaiToken expect = tests[i];
    DaiToken *tok = DaiTokenList_next(list);
    munit_assert_int(expect.type, ==, tok->type);
    munit_assert_string_equal(expect.literal, tok->literal);
    munit_assert_int(expect.start_line, ==, tok->start_line);
    munit_assert_int(expect.start_column, ==, tok->start_column);
    munit_assert_int(tok->start_line, ==, tok->end_line);
    munit_assert_int(tok->start_column + dai_utf8_strlen(tok->literal), ==,
                     tok->end_column);
  }
  DaiTokenList_free(list);
  unlink(filename);

  return MUNIT_OK;
}

static MunitResult test_illegal_token(__attribute__((unused))
                                      const MunitParameter params[],
                                      __attribute__((unused)) void *user_data) {
  struct {
    const char *input;
    const char *error_message;
  } tests[] = {
      {"$$$", "SyntaxError: illegal character '$' in <stdin>:1:1"},
      {"\xbf", "SyntaxError: invalid utf8 encoding character in <stdin>:1:1"},
      {"'a\n'", "SyntaxError: unclosed string literal in <stdin>:1:3"},
  };
  for (int i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
    const char *input = tests[i].input;
    DaiTokenList list;
    DaiSyntaxError *err = dai_tokenize_string(input, &list);
    DaiSyntaxError_setFilename(err, "<stdin>");
    munit_assert_not_null(err);
    {
      char *s = DaiSyntaxError_string(err);
      munit_assert_string_equal(s, tests[i].error_message);
      free(s);
    }
    DaiTokenList_reset(&list);
    DaiSyntaxError_free(err);
  }
  return MUNIT_OK;
}

MunitTest tokenize_suite_tests[] = {
    {(char *)"/test_next_token", test_next_token, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {(char *)"/test_tokenize_file", test_tokenize_file, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {(char *)"/test_illegal_token", test_illegal_token, NULL, NULL,
     MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};
