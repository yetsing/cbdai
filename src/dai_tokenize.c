//
// Created by  on 2024/5/28.
//

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dai_codecs.h"
#include "dai_malloc.h"
#include "dai_tokenize.h"
#include "dai_windows.h"   // IWYU pragma: keep

// #region DaiToken 辅助变量和函数

#define TOKEN_LITERAL(ty, val) {ty, val, .length = strlen(val)}

// Token 类型字符串数组
static const char* DaiTokenTypeStrings[] = {
    "DaiTokenType_illegal",     "DaiTokenType_eof",         "DaiTokenType_ident",
    "DaiTokenType_int",         "DaiTokenType_float",       "DaiTokenType_comment",
    "DaiTokenType_str",         "DaiTokenType_function",    "DaiTokenType_var",
    "DaiTokenType_con",         "DaiTokenType_true",        "DaiTokenType_false",
    "DaiTokenType_nil",         "DaiTokenType_if",          "DaiTokenType_elif",
    "DaiTokenType_else",        "DaiTokenType_return",      "DaiTokenType_class",
    "DaiTokenType_self",        "DaiTokenType_super",       "DaiTokenType_for",
    "DaiTokenType_in",          "DaiTokenType_while",       "DaiTokenType_break",
    "DaiTokenType_continue",    "DaiTokenType_and",         "DaiTokenType_or",
    "DaiTokenType_not",         "DaiTokenType_auto",        "DaiTokenType_assign",
    "DaiTokenType_plus",        "DaiTokenType_minus",       "DaiTokenType_bang",
    "DaiTokenType_asterisk",    "DaiTokenType_slash",       "DaiTokenType_percent",
    "DaiTokenType_left_shift",  "DaiTokenType_right_shift", "DaiTokenType_bitwise_and",
    "DaiTokenType_bitwise_xor", "DaiTokenType_bitwise_or",  "DaiTokenType_bitwise_not",
    "DaiTokenType_lt",          "DaiTokenType_gt",          "DaiTokenType_lte",
    "DaiTokenType_gte",         "DaiTokenType_eq",          "DaiTokenType_not_eq",
    "DaiTokenType_dot",         "DaiTokenType_comma",       "DaiTokenType_semicolon",
    "DaiTokenType_colon",       "DaiTokenType_lparen",      "DaiTokenType_rparen",
    "DaiTokenType_lbracket",    "DaiTokenType_rbracket",    "DaiTokenType_lbrace",
    "DaiTokenType_rbrace",      "DaiTokenType_add_assign",  "DaiTokenType_sub_assign",
    "DaiTokenType_mul_assign",  "DaiTokenType_div_assign",  "DaiTokenType_end",
};

// 返回 Token 类型的字符串表示
__attribute__((unused)) const char*
DaiTokenType_string(const DaiTokenType type) {
    return DaiTokenTypeStrings[type];
}

// DaiToken_auto 类型映射
static DaiToken autos[] = {
    TOKEN_LITERAL(DaiTokenType_assign, "="),       TOKEN_LITERAL(DaiTokenType_plus, "+"),
    TOKEN_LITERAL(DaiTokenType_minus, "-"),        TOKEN_LITERAL(DaiTokenType_bang, "!"),
    TOKEN_LITERAL(DaiTokenType_asterisk, "*"),     TOKEN_LITERAL(DaiTokenType_slash, "/"),
    TOKEN_LITERAL(DaiTokenType_percent, "%"),      TOKEN_LITERAL(DaiTokenType_left_shift, "<<"),
    TOKEN_LITERAL(DaiTokenType_right_shift, ">>"), TOKEN_LITERAL(DaiTokenType_bitwise_and, "&"),
    TOKEN_LITERAL(DaiTokenType_bitwise_or, "|"),   TOKEN_LITERAL(DaiTokenType_bitwise_not, "~"),
    TOKEN_LITERAL(DaiTokenType_bitwise_xor, "^"),

    TOKEN_LITERAL(DaiTokenType_lt, "<"),           TOKEN_LITERAL(DaiTokenType_gt, ">"),
    TOKEN_LITERAL(DaiTokenType_lte, "<="),         TOKEN_LITERAL(DaiTokenType_gte, ">="),
    TOKEN_LITERAL(DaiTokenType_not_eq, "!="),      TOKEN_LITERAL(DaiTokenType_eq, "=="),

    TOKEN_LITERAL(DaiTokenType_dot, "."),          TOKEN_LITERAL(DaiTokenType_comma, ","),
    TOKEN_LITERAL(DaiTokenType_semicolon, ";"),    TOKEN_LITERAL(DaiTokenType_colon, ":"),

    TOKEN_LITERAL(DaiTokenType_lparen, "("),       TOKEN_LITERAL(DaiTokenType_rparen, ")"),
    TOKEN_LITERAL(DaiTokenType_lbrace, "{"),       TOKEN_LITERAL(DaiTokenType_rbrace, "}"),
    TOKEN_LITERAL(DaiTokenType_lbracket, "["),     TOKEN_LITERAL(DaiTokenType_rbracket, "]"),

    TOKEN_LITERAL(DaiTokenType_add_assign, "+="),  TOKEN_LITERAL(DaiTokenType_sub_assign, "-="),
    TOKEN_LITERAL(DaiTokenType_mul_assign, "*="),  TOKEN_LITERAL(DaiTokenType_div_assign, "/="),
};

// DaiTokenType_auto 类型转换
void
Token_autoConvert(DaiToken* t) {
    assert(t->type == DaiTokenType_auto);
    for (size_t i = 0; i < sizeof(autos) / sizeof(autos[0]); i++) {
        if (t->length == autos[i].length && strncmp(t->s, autos[i].s, t->length) == 0) {
            t->type = autos[i].type;
            break;
        }
    }
}

// 关键字 Token
static DaiToken keywords[] = {
    TOKEN_LITERAL(DaiTokenType_function, "fn"), TOKEN_LITERAL(DaiTokenType_var, "var"),
    TOKEN_LITERAL(DaiTokenType_con, "con"),     TOKEN_LITERAL(DaiTokenType_true, "true"),
    TOKEN_LITERAL(DaiTokenType_false, "false"), TOKEN_LITERAL(DaiTokenType_nil, "nil"),
    TOKEN_LITERAL(DaiTokenType_if, "if"),       TOKEN_LITERAL(DaiTokenType_elif, "elif"),
    TOKEN_LITERAL(DaiTokenType_else, "else"),   TOKEN_LITERAL(DaiTokenType_return, "return"),
    TOKEN_LITERAL(DaiTokenType_class, "class"), TOKEN_LITERAL(DaiTokenType_self, "self"),
    TOKEN_LITERAL(DaiTokenType_super, "super"), TOKEN_LITERAL(DaiTokenType_for, "for"),
    TOKEN_LITERAL(DaiTokenType_in, "in"),       TOKEN_LITERAL(DaiTokenType_while, "while"),
    TOKEN_LITERAL(DaiTokenType_break, "break"), TOKEN_LITERAL(DaiTokenType_continue, "continue"),
    TOKEN_LITERAL(DaiTokenType_and, "and"),     TOKEN_LITERAL(DaiTokenType_or, "or"),
    TOKEN_LITERAL(DaiTokenType_not, "not"),
};

// 查询关键字类型
static DaiTokenType
lookup_ident(const char* ident, size_t length) {
    for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); ++i) {
        if (length == keywords[i].length && strncmp(ident, keywords[i].s, length) == 0) {
            return keywords[i].type;
        }
    }
    return DaiTokenType_ident;
}

// #endregion

// #region DaiTokenList 结构体及其方法

// 初始化 DaiTokenList 结构体
void
DaiTokenList_init(DaiTokenList* list) {
    list->index  = 0;
    list->length = 0;
    list->size   = 0;
    list->tokens = NULL;
}

// 重置 DaiTokenList 结构体，释放内存
void
DaiTokenList_reset(DaiTokenList* list) {
    dai_free(list->tokens);
    DaiTokenList_init(list);
}

// 返回当前 token 并将读取位置加一
DaiToken*
DaiTokenList_next(DaiTokenList* list) {
    if (list->index >= list->length) {
        return &(list->tokens[list->length - 1]);
    }
    DaiToken* t = &(list->tokens[list->index]);
    list->index++;
    return t;
}

// 返回 token 列表的长度
size_t
DaiTokenList_length(const DaiTokenList* list) {
    return list->length;
}

// 返回当前 token 的索引
size_t
DaiTokenList_current_index(const DaiTokenList* list) {
    return list->index - 1;
}

// 返回指定索引的 token
DaiToken*
DaiTokenList_get(const DaiTokenList* list, size_t index) {
    assert(index >= 0 && index < list->length);
    return &list->tokens[index];
}

// 扩展 token 列表的大小
static void
DaiTokenList_grow(DaiTokenList* list, size_t size) {
    list->size   = size;
    list->tokens = dai_realloc(list->tokens, sizeof(DaiToken) * list->size);
}

static DaiToken*
DaiTokenList_new_token(DaiTokenList* list) {
    if (list->length >= list->size) {
        // 因为 Tokenizer_new 可能会初始化一个比较大的 token 列表，所以这里不需要通常的倍数增长
        // 32 是拍脑袋定的一个数值
        size_t new_size = list->size + 32;
        DaiTokenList_grow(list, new_size);
    }
    DaiToken* tok = &(list->tokens[list->length]);
    tok->index    = list->length;
    list->length++;
    return tok;
}

// #endregion

// #region Tokenizer 分词器，进行词法分析
typedef struct {
    const char* s;
    size_t number_of_byte;   // 源字符串的字节数
    dai_rune_t ch;           // 当前 unicode 字符
    size_t byte_of_ch;       // 当前字符的字节数
    size_t position;         // 指向当前字符(以字节计数)
    size_t read_position;    // 指向下一个字符（以字节计数）

    size_t mark_position;   // 标记位置（以字节计数）
    int mark_line;          // 标记行
    int mark_column;        // 标记列

    int line;     // 当前行
    int column;   // 当前列

    // Tokenizer_run 会创建通用的错误消息，有时需要一些特定的错误消息，更好地表达错误原因
    // 这些错误消息不好从深层的分析函数中返回，所以在这里记录一下
    bool has_error_msg;    // 是否以构建错误消息
    char error_msg[128];   // 错误消息

    DaiTokenList* tlist;   // token 列表，只是一个引用，不管理内存
} Tokenizer;

// #region Tokenizer 辅助方法

// 读取下一个字符
// 读取字符时会更新行和列信息
// 如果读取到文件末尾，则将 ch 设置为 0
// 如果读取到无效的 utf8 编码，则设置 has_error_msg 为 true，并设置 error_msg
static void
Tokenizer_read_char(Tokenizer* tker) {
    if (tker->ch == '\n') {
        tker->line++;
        tker->column = 0;
    }
    int increment = 1;
    if (tker->read_position >= tker->number_of_byte) {
        tker->ch         = 0;
        tker->byte_of_ch = 0;
    } else {
        // utf8 解码
        int count_of_byte = dai_utf8_decode(tker->s + tker->read_position, &tker->ch);
        if (count_of_byte > 0) {
            increment        = count_of_byte;
            tker->byte_of_ch = count_of_byte;
        } else {
            tker->has_error_msg = true;
            snprintf(tker->error_msg, sizeof(tker->error_msg), "invalid utf8 encoding character");
        }
    }
    tker->position = tker->read_position;
    tker->read_position += increment;
    tker->column++;
}

// 返回下一个字节
static char
Tokenizer_peek_char(Tokenizer* tker) {
    if (tker->read_position >= tker->number_of_byte) {
        // 返回 0 表示 EOF
        return 0;
    } else {
        return tker->s[tker->read_position];
    }
}

// 标记当前位置
static void
Tokenizer_mark(Tokenizer* tker) {
    tker->mark_position = tker->position;
    tker->mark_line     = tker->line;
    tker->mark_column   = tker->column;
}

// 跳过空白字符
// 空白字符包括空格、制表符、换行符和回车符
static void
Tokenizer_skip_whitespace(Tokenizer* tker) {
    while (tker->ch == ' ' || tker->ch == '\t' || tker->ch == '\n' || tker->ch == '\r') {
        Tokenizer_read_char(tker);
    }
}

static DaiToken*
Tokenizer_new_token(Tokenizer* tker) {
    return DaiTokenList_new_token(tker->tlist);
}

// 构建一个新的 Token
static DaiToken*
Tokenizer_build_token(Tokenizer* tker, DaiTokenType type) {
    assert(tker->position >= tker->mark_position);
    DaiToken* tok     = Tokenizer_new_token(tker);
    tok->type         = type;
    tok->s            = tker->s + tker->mark_position;
    tok->length       = tker->position - tker->mark_position;
    tok->start_line   = tker->mark_line;
    tok->start_column = tker->mark_column;
    tok->end_line     = tker->line;
    tok->end_column   = tker->column;
    if (tok->type == DaiTokenType_auto) {
        Token_autoConvert(tok);
    }
    return tok;
}

// #endregion

// 创建一个新的 Tokenizer
// 参数 s 是源字符串，返回一个新的 Tokenizer 对象
// 注意：调用者需要负责释放 Tokenizer 对象
static Tokenizer*
Tokenizer_New(const char* s, DaiTokenList* tlist) {
    Tokenizer* tker      = dai_malloc(sizeof(Tokenizer));
    tker->s              = s;
    tker->number_of_byte = strlen(s);

    tker->ch            = 0;
    tker->byte_of_ch    = 0;
    tker->position      = 0;
    tker->read_position = 0;
    tker->mark_position = 0;

    tker->line   = 1;
    tker->column = 0;

    tker->has_error_msg = false;
    memset(tker->error_msg, 0, sizeof(tker->error_msg));

    tker->tlist = tlist;
    DaiTokenList_grow(tlist, tker->number_of_byte / 8);   // 预分配内存，大小是拍脑袋定的

    Tokenizer_read_char(tker);
    return tker;
}

// 释放 Tokenizer 对象
static void
Tokenizer_free(Tokenizer* tker) {
    dai_free(tker);
}

// #region token 解析逻辑
static bool
is_digit(const dai_rune_t c) {
    return '0' <= c && c <= '9';
}

static bool
is_bindigit(const dai_rune_t c) {
    return c == '0' || c == '1';
}

static bool
is_octdigit(const dai_rune_t c) {
    return '0' <= c && c <= '7';
}

static bool
is_hexdigit(const dai_rune_t c) {
    return ('0' <= c && c <= '9') || ('a' <= c && c <= 'f') || ('A' <= c && c <= 'F');
}

// 解析小数点后面部分
static DaiToken*
Tokenizer_read_fraction(Tokenizer* tker) {
    Tokenizer_read_char(tker);
    // fraction 部分
    if (!is_digit(tker->ch)) {
        tker->has_error_msg = true;
        snprintf(tker->error_msg, sizeof(tker->error_msg), "invalid number");
        return Tokenizer_build_token(tker, DaiTokenType_illegal);
    }
    Tokenizer_read_char(tker);
    while (is_digit(tker->ch) || tker->ch == '_') {
        while (tker->ch == '_') {
            Tokenizer_read_char(tker);
        }
        if (!is_digit(tker->ch)) {
            tker->has_error_msg = true;
            snprintf(tker->error_msg, sizeof(tker->error_msg), "invalid number");
            return Tokenizer_build_token(tker, DaiTokenType_illegal);
        }
        Tokenizer_read_char(tker);
    }
    if (tker->ch == 'e' || tker->ch == 'E') {
        // exp 部分
        Tokenizer_read_char(tker);
        if (tker->ch == '+' || tker->ch == '-') {
            Tokenizer_read_char(tker);
        }
        do {
            if (!is_digit(tker->ch)) {
                tker->has_error_msg = true;
                snprintf(tker->error_msg, sizeof(tker->error_msg), "invalid number");
                return Tokenizer_build_token(tker, DaiTokenType_illegal);
            }
            Tokenizer_read_char(tker);
        } while (is_digit(tker->ch));
    }
    return Tokenizer_build_token(tker, DaiTokenType_float);
}

// 数字字面量语法
// 参考： https://docs.python.org/3/reference/lexical_analysis.html#integer-literals
// 下划线不能出现在数字的开头或结尾，也不能出现在小数点前后
// number     ::= zero | decinteger | bininteger | octinteger | hexinteger | float
// zero       ::= "0"
// decinteger ::= nonzerodigit ( ("_")* digit)*
// bininteger ::= "0" ("b" | "B") ( ("_")* bindigit)+
// octinteger ::= "0" ("o" | "O") ( ("_")* octdigit)+
// hexinteger ::= "0" ("x" | "X") ( ("_")* hexdigit)+
// float      ::= ("0" | decinteger) "." fraction [ exp ]
//
// fraction     ::= digit ( ("_")* digit)*
// exp          ::= ("e" | "E") [ "-" | "+" ] (digit)+
// nonzerodigit ::=  "1"..."9"
// digit        ::=  "0"..."9"
// bindigit     ::=  "0" | "1"
// octdigit     ::=  "0"..."7"
// hexdigit     ::=  digit | "a"..."f" | "A"..."F"
static DaiToken*
Tokenizer_read_number(Tokenizer* tker) {
    if (tker->ch == '0') {
        // zero | bininteger | octinteger | hexinteger | float
        Tokenizer_read_char(tker);
        switch (tker->ch) {
            case 'b':
            case 'B': {
                Tokenizer_read_char(tker);
                do {
                    while (tker->ch == '_') {
                        Tokenizer_read_char(tker);
                    }
                    if (!is_bindigit(tker->ch)) {
                        tker->has_error_msg = true;
                        snprintf(tker->error_msg, sizeof(tker->error_msg), "invalid number");
                        return Tokenizer_build_token(tker, DaiTokenType_illegal);
                    }
                    Tokenizer_read_char(tker);
                } while (is_bindigit(tker->ch) || tker->ch == '_');
                break;
            }
            case 'o':
            case 'O': {
                Tokenizer_read_char(tker);
                do {
                    while (tker->ch == '_') {
                        Tokenizer_read_char(tker);
                    }
                    if (!is_octdigit(tker->ch)) {
                        tker->has_error_msg = true;
                        snprintf(tker->error_msg, sizeof(tker->error_msg), "invalid number");
                        return Tokenizer_build_token(tker, DaiTokenType_illegal);
                    }
                    Tokenizer_read_char(tker);
                } while (is_octdigit(tker->ch) || tker->ch == '_');
                break;
            }
            case 'x':
            case 'X': {
                Tokenizer_read_char(tker);
                do {
                    while (tker->ch == '_') {
                        Tokenizer_read_char(tker);
                    }
                    if (!is_hexdigit(tker->ch)) {
                        tker->has_error_msg = true;
                        snprintf(tker->error_msg, sizeof(tker->error_msg), "invalid number");
                        return Tokenizer_build_token(tker, DaiTokenType_illegal);
                    }
                    Tokenizer_read_char(tker);
                } while (is_hexdigit(tker->ch) || tker->ch == '_');
                break;
            }
            case '.': {
                // float 情况
                return Tokenizer_read_fraction(tker);
                break;
            }
            default: {
                if (is_digit(tker->ch) || tker->ch == '_') {
                    tker->has_error_msg = true;
                    snprintf(tker->error_msg,
                             sizeof(tker->error_msg),
                             "leading zeros in decimal integer literals are not permitted");
                    return Tokenizer_build_token(tker, DaiTokenType_illegal);
                }
                break;
            }
        }
        return Tokenizer_build_token(tker, DaiTokenType_int);
    }

    // decinteger | float
    Tokenizer_read_char(tker);
    while (is_digit(tker->ch) || tker->ch == '_') {
        while (tker->ch == '_') {
            Tokenizer_read_char(tker);
        }
        if (!is_digit(tker->ch)) {
            tker->has_error_msg = true;
            snprintf(tker->error_msg, sizeof(tker->error_msg), "invalid number");
            return Tokenizer_build_token(tker, DaiTokenType_illegal);
        }
        Tokenizer_read_char(tker);
    }
    if (tker->ch == '.') {
        // float 情况
        return Tokenizer_read_fraction(tker);
    }
    return Tokenizer_build_token(tker, DaiTokenType_int);
}

static bool
is_unicode_letter(dai_rune_t c) {
    // Basic Latin letters
    if (('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z')) {
        return true;
    }

    // CJK Unified Ideographs (Common Chinese characters)
    if (0x4E00 <= c && c <= 0x9FFF) {
        return true;
    }

    // CJK Extension A
    if (0x3400 <= c && c <= 0x4DBF) {
        return true;
    }

    // Hiragana
    if (0x3040 <= c && c <= 0x309F) {
        return true;
    }

    // Katakana
    if (0x30A0 <= c && c <= 0x30FF) {
        return true;
    }

    // Latin-1 Supplement letters
    if ((0xC0 <= c && c <= 0xD6) || (0xD8 <= c && c <= 0xF6) || (0xF8 <= c && c <= 0xFF)) {
        return true;
    }

    // Other scripts
    if ((0x0100 <= c && c <= 0x02AF) ||   // Extended Latin
        (0x0370 <= c && c <= 0x037D) ||   // Greek
        (0x037F <= c && c <= 0x1FFF)) {   // More Greek, Cyrillic
        return true;
    }

    return false;
}

static bool
is_unicode_number(dai_rune_t c) {
    // Basic digits
    if ('0' <= c && c <= '9') {
        return true;
    }

    // Other number forms (simplified)
    if ((0x0660 <= c && c <= 0x0669) ||   // Arabic-Indic digits
        (0x06F0 <= c && c <= 0x06F9) ||   // Extended Arabic-Indic digits
        (0x07C0 <= c && c <= 0x07C9) ||   // NKo digits
        (0x0966 <= c && c <= 0x096F)) {   // Devanagari digits
        return true;
    }

    return false;
}

// 判断字符是否是标识符的起始字符
static bool
is_identifier_start(dai_rune_t rune) {
    if (rune == '_') {
        return true;
    }
    return is_unicode_letter(rune);
}

// 判断字符是否是标识符的后续字符
static bool
is_identifier_continue(dai_rune_t rune) {
    if (rune == '_') {
        return true;
    }
    return is_unicode_letter(rune) || is_unicode_number(rune);
}

// 读取标识符
static DaiToken*
Tokenizer_read_identifier(Tokenizer* tker) {
    // identifier 允许的字符
    // https://docs.python.org/3/reference/lexical_analysis.html#identifiers
    while (is_identifier_continue(tker->ch)) {
        Tokenizer_read_char(tker);
    }
    DaiToken* tok = Tokenizer_build_token(tker, DaiTokenType_ident);
    tok->type     = lookup_ident(tok->s, tok->length);
    return tok;
}


// 读取注释
// 以 // 开头的单行注释
// 以 # 开头的单行注释
static DaiToken*
Tokenizer_read_comment(Tokenizer* tker) {
    while (tker->ch != '\n' && tker->ch != 0) {
        Tokenizer_read_char(tker);
    }
    return Tokenizer_build_token(tker, DaiTokenType_comment);
}


// 读取字符串
// "" '' 之间的单行字符串
// `` 之间的多行字符串
static DaiToken*
Tokenizer_readString(Tokenizer* tker, const dai_rune_t quote) {
    bool multiline = quote == '`';
    Tokenizer_read_char(tker);
    while (tker->ch != quote && tker->ch != 0 && (tker->ch != '\n' || multiline)) {
        switch (tker->ch) {
            case '\\': {
                Tokenizer_read_char(tker);
                switch (tker->ch) {
                    case '\\': Tokenizer_read_char(tker); break;
                    case 'n': Tokenizer_read_char(tker); break;
                    case 't': Tokenizer_read_char(tker); break;
                    case 'r': Tokenizer_read_char(tker); break;
                    case '"': Tokenizer_read_char(tker); break;
                    case '\'': Tokenizer_read_char(tker); break;
                    case 'x': {
                        // \xXX 十六进制转义
                        for (size_t i = 0; i < 2; i++) {
                            Tokenizer_read_char(tker);
                            if (!is_hexdigit(tker->ch)) {
                                tker->has_error_msg = true;
                                snprintf(tker->error_msg,
                                         sizeof(tker->error_msg),
                                         "invalid \\xXX escape");
                                Tokenizer_mark(tker);
                                return Tokenizer_build_token(tker, DaiTokenType_illegal);
                            }
                        }
                        Tokenizer_read_char(tker);
                        break;
                    }
                    case 0: {
                        goto LOOP_END;
                    }
                    default: {
                        tker->has_error_msg = true;
                        snprintf(tker->error_msg,
                                 sizeof(tker->error_msg),
                                 "invalid escape %d",
                                 tker->ch);
                        Tokenizer_mark(tker);
                        return Tokenizer_build_token(tker, DaiTokenType_illegal);
                        break;
                    }
                }
                break;
            }
            default: Tokenizer_read_char(tker); break;
        }
    }
LOOP_END:
    if (tker->ch != quote) {
        tker->has_error_msg = true;
        snprintf(tker->error_msg, sizeof(tker->error_msg), "unclosed string literal");
        Tokenizer_mark(tker);
        return Tokenizer_build_token(tker, DaiTokenType_illegal);
    } else {
        Tokenizer_read_char(tker);
        return Tokenizer_build_token(tker, DaiTokenType_str);
    }
}

// 读取下一个 token
// 读取下一个 token 时会跳过空白字符
static DaiToken*
Tokenizer_next_token(Tokenizer* tker) {
    Tokenizer_skip_whitespace(tker);
    Tokenizer_mark(tker);
    dai_rune_t ch = tker->ch;
    switch (ch) {
        case ':':
        case '^':
        case '&':
        case '|':
        case '~':
        case '.':
        case ',':
        case ';':
        case '{':
        case '}':
        case '(':
        case ')':
        case '[':
        case ']':
        case '%': {
            Tokenizer_read_char(tker);
            return Tokenizer_build_token(tker, DaiTokenType_auto);
        }
        case '+': {
            Tokenizer_read_char(tker);
            if (tker->ch == '=') {
                Tokenizer_read_char(tker);
                return Tokenizer_build_token(tker, DaiTokenType_add_assign);
            }
            return Tokenizer_build_token(tker, DaiTokenType_plus);
        }
        case '-': {
            Tokenizer_read_char(tker);
            if (tker->ch == '=') {
                Tokenizer_read_char(tker);
                return Tokenizer_build_token(tker, DaiTokenType_sub_assign);
            }
            return Tokenizer_build_token(tker, DaiTokenType_minus);
        }
        case '*': {
            Tokenizer_read_char(tker);
            if (tker->ch == '=') {
                Tokenizer_read_char(tker);
                return Tokenizer_build_token(tker, DaiTokenType_mul_assign);
            }
            return Tokenizer_build_token(tker, DaiTokenType_asterisk);
        }
        case '/': {
            if (ch == '/' && Tokenizer_peek_char(tker) == '/') {
                // consume both slashes, then read to end-of-line
                Tokenizer_read_char(tker);   // first slash
                Tokenizer_read_char(tker);   // second slash
                return Tokenizer_read_comment(tker);
            } else if (Tokenizer_peek_char(tker) == '=') {
                Tokenizer_read_char(tker);
                Tokenizer_read_char(tker);
                return Tokenizer_build_token(tker, DaiTokenType_div_assign);
            } else {
                Tokenizer_read_char(tker);
                return Tokenizer_build_token(tker, DaiTokenType_auto);
            }
        }
        case '!': {
            if (Tokenizer_peek_char(tker) == '=') {
                Tokenizer_read_char(tker);
                Tokenizer_read_char(tker);
                return Tokenizer_build_token(tker, DaiTokenType_not_eq);
            } else {
                Tokenizer_read_char(tker);
                return Tokenizer_build_token(tker, DaiTokenType_auto);
            }
        }
        case '=': {
            if (Tokenizer_peek_char(tker) == '=') {
                Tokenizer_read_char(tker);
                Tokenizer_read_char(tker);
                return Tokenizer_build_token(tker, DaiTokenType_eq);
            } else {
                Tokenizer_read_char(tker);
                return Tokenizer_build_token(tker, DaiTokenType_auto);
            }
        }
        case '<': {
            if (Tokenizer_peek_char(tker) == '=') {
                Tokenizer_read_char(tker);
                Tokenizer_read_char(tker);
                return Tokenizer_build_token(tker, DaiTokenType_lte);
            } else if (Tokenizer_peek_char(tker) == '<') {
                Tokenizer_read_char(tker);
                Tokenizer_read_char(tker);
                return Tokenizer_build_token(tker, DaiTokenType_left_shift);
            }
            Tokenizer_read_char(tker);
            return Tokenizer_build_token(tker, DaiTokenType_auto);
        }
        case '>': {
            if (Tokenizer_peek_char(tker) == '=') {
                Tokenizer_read_char(tker);
                Tokenizer_read_char(tker);
                return Tokenizer_build_token(tker, DaiTokenType_gte);
            } else if (Tokenizer_peek_char(tker) == '>') {
                Tokenizer_read_char(tker);
                Tokenizer_read_char(tker);
                return Tokenizer_build_token(tker, DaiTokenType_right_shift);
            }
            Tokenizer_read_char(tker);
            return Tokenizer_build_token(tker, DaiTokenType_auto);
        }
        case '#': {
            return Tokenizer_read_comment(tker);
        }
        case '`':
        case '\'':
        case '"': {
            return Tokenizer_readString(tker, ch);
        }
        case 0: {
            return Tokenizer_build_token(tker, DaiTokenType_eof);
        }
        default: {
            if (is_identifier_start(ch)) {
                return Tokenizer_read_identifier(tker);
            } else if (is_digit(ch)) {
                return Tokenizer_read_number(tker);
            } else {
                Tokenizer_read_char(tker);
                return Tokenizer_build_token(tker, DaiTokenType_illegal);
            }
        }
    }
}
// #endregion

// 执行词法分析
static DaiSyntaxError*
Tokenizer_run(Tokenizer* tker, DaiTokenList* tlist) {
    while (true) {
        size_t prev_position = tker->position;
        DaiToken* tok        = Tokenizer_next_token(tker);
        switch (tok->type) {
            case DaiTokenType_illegal: {
                if (!tker->has_error_msg) {
                    snprintf(tker->error_msg,
                             sizeof(tker->error_msg),
                             "illegal character '%.*s'",
                             (int)tok->length,
                             tok->s);
                }
                return DaiSyntaxError_New(tker->error_msg, tok->start_line, tok->start_column);
            }
            case DaiTokenType_eof: {
                return NULL;
            }
            default: {
                // 断言读取位置在向前推进
                assert(tker->position > prev_position);
                break;
            }
        }
    }
    return NULL;
}

// #endregion

DaiSyntaxError*
dai_tokenize_string(const char* s, DaiTokenList* tlist) {
    Tokenizer* tker     = Tokenizer_New(s, tlist);
    DaiSyntaxError* err = Tokenizer_run(tker, tlist);
    Tokenizer_free(tker);
    return err;
}