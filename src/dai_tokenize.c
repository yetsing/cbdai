//
// Created by  on 2024/5/28.
//

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ucd/ucd.h"

#include "dai_codecs.h"
#include "dai_malloc.h"
#include "dai_tokenize.h"

static const char* DaiTokenTypeStrings[] = {
    "DaiTokenType_illegal", "DaiTokenType_eof",      "DaiTokenType_ident",
    "DaiTokenType_int",     "DaiTokenType_float",    "DaiTokenType_comment",
    "DaiTokenType_str",     "DaiTokenType_function", "DaiTokenType_var",
    "DaiTokenType_true",    "DaiTokenType_false",    "DaiTokenType_nil",
    "DaiTokenType_if",      "DaiTokenType_elif",     "DaiTokenType_else",
    "DaiTokenType_return",  "DaiTokenType_class",    "DaiTokenType_self",
    "DaiTokenType_super",   "DaiTokenType_for",      "DaiTokenType_in",
    "DaiTokenType_while",   "DaiTokenType_break",    "DaiTokenType_continue",
    "DaiTokenType_auto",    "DaiTokenType_assign",   "DaiTokenType_plus",
    "DaiTokenType_minus",   "DaiTokenType_bang",     "DaiTokenType_asterisk",
    "DaiTokenType_slash",   "DaiTokenType_percent",  "DaiTokenType_lt",
    "DaiTokenType_gt",      "DaiTokenType_eq",       "DaiTokenType_not_eq",
    "DaiTokenType_dot",     "DaiTokenType_comma",    "DaiTokenType_semicolon",
    "DaiTokenType_lparen",  "DaiTokenType_rparen",   "DaiTokenType_lbrace",
    "DaiTokenType_rbrace",  "DaiTokenType_end",
};


__attribute__((unused)) const char*
DaiTokenType_string(DaiTokenType type) {
    return DaiTokenTypeStrings[type];
}

static DaiToken autos[] = {
    {DaiTokenType_assign, "="},
    {DaiTokenType_plus, "+"},
    {DaiTokenType_minus, "-"},
    {DaiTokenType_asterisk, "*"},
    {DaiTokenType_slash, "/"},
    {DaiTokenType_comma, ","},
    {DaiTokenType_semicolon, ";"},
    {DaiTokenType_lparen, "("},
    {DaiTokenType_rparen, ")"},
    {DaiTokenType_lbrace, "{"},
    {DaiTokenType_rbrace, "}"},
    {DaiTokenType_bang, "!"},
    {DaiTokenType_dot, "."},
    {DaiTokenType_percent, "%"},
};

static DaiToken keywords[] = {
    {DaiTokenType_function, "fn"},
    {DaiTokenType_var, "var"},
    {DaiTokenType_true, "true"},
    {DaiTokenType_false, "false"},
    {DaiTokenType_nil, "nil"},
    {DaiTokenType_if, "if"},
    {DaiTokenType_elif, "elif"},
    {DaiTokenType_else, "else"},
    {DaiTokenType_return, "return"},
    {DaiTokenType_class, "class"},
    {DaiTokenType_self, "self"},
    {DaiTokenType_super, "super"},
    {DaiTokenType_for, "for"},
    {DaiTokenType_in, "in"},
    {DaiTokenType_while, "while"},
    {DaiTokenType_break, "break"},
    {DaiTokenType_continue, "continue"},
};

void
Token_autoConvert(DaiToken* t) {
    assert(t->type == DaiTokenType_auto);
    for (size_t i = 0; i < sizeof(autos) / sizeof(autos[0]); i++) {
        if (strcmp(t->literal, autos[i].literal) == 0) {
            t->type = autos[i].type;
            break;
        }
    }
}

// #region DaiTokenList 结构体及其方法

void
DaiTokenList_init(DaiTokenList* list) {
    list->index  = 0;
    list->length = 0;
    list->tokens = NULL;
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

size_t
DaiTokenList_length(const DaiTokenList* list) {
    return list->length;
}

void
DaiTokenList_reset(DaiTokenList* list) {
    // 释放 token.literal
    for (int i = 0; i < list->length; ++i) {
        if (list->tokens[i].literal != NULL) {
            dai_free(list->tokens[i].literal);
        }
    }
    dai_free(list->tokens);
    DaiTokenList_init(list);
}

DaiTokenList*
DaiTokenList_New() {
    DaiTokenList* list = (DaiTokenList*)dai_malloc(sizeof(DaiTokenList));
    DaiTokenList_init(list);
    return list;
}

void
DaiTokenList_free(DaiTokenList* list) {
    // 释放 token.literal
    for (int i = 0; i < list->length; ++i) {
        if (list->tokens[i].literal != NULL) {
            dai_free(list->tokens[i].literal);
        }
    }
    dai_free(list->tokens);
    dai_free(list);
}

// #endregion

// #region Tokenizer 分词器，进行词法分析
typedef struct {
    const char* s;
    size_t slength;
    dai_rune_t ch;          // 当前字符
    size_t position;        // 指向当前字符
    size_t read_position;   // 指向下一个字符

    size_t mark_position;
    int mark_line;
    int mark_column;

    int line;
    int column;

    bool has_error_msg;
    char error_msg[128];

    size_t tokens_offset;
    size_t tokens_size;
    DaiToken* tokens;
} Tokenizer;

void
Tokenizer_readChar(Tokenizer* tker) {
    if (tker->ch == '\n') {
        tker->line++;
        tker->column = 0;
    }
    int increment = 1;
    if (tker->read_position >= tker->slength) {
        tker->ch = 0;
    } else {
        int countOfBytes = dai_utf8_decode(tker->s + tker->read_position, &tker->ch);
        if (countOfBytes > 0) {
            increment = countOfBytes;
        } else {
            tker->has_error_msg = true;
            snprintf(tker->error_msg, sizeof(tker->error_msg), "invalid utf8 encoding character");
        }
    }
    tker->position = tker->read_position;
    tker->read_position += increment;
    tker->column++;
}

char
Tokenizer_peekChar(Tokenizer* tker) {
    if (tker->read_position >= tker->slength) {
        return 0;
    } else {
        return tker->s[tker->read_position];
    }
}

void
Tokenizer_mark(Tokenizer* tker) {
    tker->mark_position = tker->position;
    tker->mark_line     = tker->line;
    tker->mark_column   = tker->column;
}

Tokenizer*
Tokenizer_New(const char* s) {
    Tokenizer* tker = dai_malloc(sizeof(Tokenizer));
    tker->s         = s;
    tker->slength   = strlen(s);

    tker->ch            = 0;
    tker->position      = 0;
    tker->read_position = 0;
    tker->mark_position = 0;

    tker->line   = 1;
    tker->column = 0;

    tker->has_error_msg = false;
    memset(tker->error_msg, 0, sizeof(tker->error_msg));

    tker->tokens_offset = 0;
    tker->tokens_size   = 1;
    tker->tokens        = dai_malloc(sizeof(DaiToken) * tker->tokens_size);
    Tokenizer_readChar(tker);
    return tker;
}

void
Tokenizer_free(Tokenizer* tker) {
    dai_free(tker);
}

void
Tokenizer_skipWhitespace(Tokenizer* tker) {
    while (tker->ch == ' ' || tker->ch == '\t' || tker->ch == '\n' || tker->ch == '\r') {
        Tokenizer_readChar(tker);
    }
}

DaiToken*
Tokenizer_buildToken(Tokenizer* tker, DaiTokenType type) {
    assert(tker->position >= tker->mark_position);
    if (tker->tokens_offset >= tker->tokens_size) {
        tker->tokens_size *= 2;
        tker->tokens = dai_realloc(tker->tokens, sizeof(DaiToken) * tker->tokens_size);
    }
    DaiToken* tok   = &(tker->tokens[tker->tokens_offset]);
    tok->type       = type;
    tok->literal    = strndup(tker->s + tker->mark_position, tker->position - tker->mark_position);
    tok->start_line = tker->mark_line;
    tok->start_column = tker->mark_column;
    tok->end_line     = tker->line;
    tok->end_column   = tker->column;
    tker->tokens_offset++;
    if (tok->type == DaiTokenType_auto) {
        Token_autoConvert(tok);
    }
    return tok;
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
Tokenizer_readFloatAfterDot(Tokenizer* tker) {
    Tokenizer_readChar(tker);
    // fraction 部分
    if (!is_digit(tker->ch)) {
        tker->has_error_msg = true;
        snprintf(tker->error_msg, sizeof(tker->error_msg), "invalid number");
        return Tokenizer_buildToken(tker, DaiTokenType_illegal);
    }
    Tokenizer_readChar(tker);
    while (is_digit(tker->ch) || tker->ch == '_') {
        while (tker->ch == '_') {
            Tokenizer_readChar(tker);
        }
        if (!is_digit(tker->ch)) {
            tker->has_error_msg = true;
            snprintf(tker->error_msg, sizeof(tker->error_msg), "invalid number");
            return Tokenizer_buildToken(tker, DaiTokenType_illegal);
        }
        Tokenizer_readChar(tker);
    }
    if (tker->ch == 'e' || tker->ch == 'E') {
        // exp 部分
        Tokenizer_readChar(tker);
        if (tker->ch == '+' || tker->ch == '-') {
            Tokenizer_readChar(tker);
        }
        do {
            if (!is_digit(tker->ch)) {
                tker->has_error_msg = true;
                snprintf(tker->error_msg, sizeof(tker->error_msg), "invalid number");
                return Tokenizer_buildToken(tker, DaiTokenType_illegal);
            }
            Tokenizer_readChar(tker);
        } while (is_digit(tker->ch));
    }
    return Tokenizer_buildToken(tker, DaiTokenType_float);
}

// 数字字面量语法
// 参考： https://docs.python.org/3/reference/lexical_analysis.html#integer-literals
// number     ::= zero | decinteger | bininteger | octinteger | hexinteger | float
// zero       ::= "0"
// decinteger ::= nonzerodigit ( ("_")* digit)*
// bininteger ::= "0" ("b" | "B") ( ("_")* bindigit)+
// octinteger ::= "0" ("o" | "O") ( ("_")* octdigit)+
// hexinteger ::= "0" ("x" | "X") ( ("_")* hexdigit)+
// float      ::= ("0" | decinteger) "." fraction [ exp ]

// fraction     ::= digit ( ("_")* digit)*
// exp          ::= ("e" | "E") [ "-" | "+" ] (digit)+
// nonzerodigit ::=  "1"..."9"
// digit        ::=  "0"..."9"
// bindigit     ::=  "0" | "1"
// octdigit     ::=  "0"..."7"
// hexdigit     ::=  digit | "a"..."f" | "A"..."F"
static DaiToken*
Tokenizer_readNumber(Tokenizer* tker) {
    if (tker->ch == '0') {
        // zero | bininteger | octinteger | hexinteger | float
        Tokenizer_readChar(tker);
        switch (tker->ch) {
            case 'b':
            case 'B': {
                Tokenizer_readChar(tker);
                do {
                    while (tker->ch == '_') {
                        Tokenizer_readChar(tker);
                    }
                    if (!is_bindigit(tker->ch)) {
                        tker->has_error_msg = true;
                        snprintf(tker->error_msg, sizeof(tker->error_msg), "invalid number");
                        return Tokenizer_buildToken(tker, DaiTokenType_illegal);
                    }
                    Tokenizer_readChar(tker);
                } while (is_bindigit(tker->ch) || tker->ch == '_');
                break;
            }
            case 'o':
            case 'O': {
                Tokenizer_readChar(tker);
                do {
                    while (tker->ch == '_') {
                        Tokenizer_readChar(tker);
                    }
                    if (!is_octdigit(tker->ch)) {
                        tker->has_error_msg = true;
                        snprintf(tker->error_msg, sizeof(tker->error_msg), "invalid number");
                        return Tokenizer_buildToken(tker, DaiTokenType_illegal);
                    }
                    Tokenizer_readChar(tker);
                } while (is_octdigit(tker->ch) || tker->ch == '_');
                break;
            }
            case 'x':
            case 'X': {
                Tokenizer_readChar(tker);
                do {
                    while (tker->ch == '_') {
                        Tokenizer_readChar(tker);
                    }
                    if (!is_hexdigit(tker->ch)) {
                        tker->has_error_msg = true;
                        snprintf(tker->error_msg, sizeof(tker->error_msg), "invalid number");
                        return Tokenizer_buildToken(tker, DaiTokenType_illegal);
                    }
                    Tokenizer_readChar(tker);
                } while (is_hexdigit(tker->ch) || tker->ch == '_');
                break;
            }
            case '.': {
                // float 情况
                return Tokenizer_readFloatAfterDot(tker);
                break;
            }
            default: {
                if (is_digit(tker->ch) || tker->ch == '_') {
                    tker->has_error_msg = true;
                    snprintf(tker->error_msg,
                             sizeof(tker->error_msg),
                             "leading zeros in decimal integer literals are not permitted");
                    return Tokenizer_buildToken(tker, DaiTokenType_illegal);
                }
                break;
            }
        }
        return Tokenizer_buildToken(tker, DaiTokenType_int);
    }

    // decinteger | float
    Tokenizer_readChar(tker);
    while (is_digit(tker->ch) || tker->ch == '_') {
        while (tker->ch == '_') {
            Tokenizer_readChar(tker);
        }
        if (!is_digit(tker->ch)) {
            tker->has_error_msg = true;
            snprintf(tker->error_msg, sizeof(tker->error_msg), "invalid number");
            return Tokenizer_buildToken(tker, DaiTokenType_illegal);
        }
        Tokenizer_readChar(tker);
    }
    if (tker->ch == '.') {
        // float 情况
        return Tokenizer_readFloatAfterDot(tker);
    }
    return Tokenizer_buildToken(tker, DaiTokenType_int);
}

__attribute__((unused)) static bool
is_letter(const dai_rune_t c) {
    return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_';
}

static DaiTokenType
lookup_ident(const char* ident) {
    for (int i = 0; i < sizeof(keywords) / sizeof(keywords[0]); ++i) {
        if (strcmp(ident, keywords[i].literal) == 0) {
            return keywords[i].type;
        }
    }
    return DaiTokenType_ident;
}

static bool
is_identifier_start(dai_rune_t rune) {
    // Lu, Ll, Lt, Lm, Lo, Nl, the underscore
    if (rune == '_') {
        return true;
    }
    ucd_category category = ucd_lookup_category(rune);
    switch (category) {
        case UCD_CATEGORY_Lu:
        case UCD_CATEGORY_Ll:
        case UCD_CATEGORY_Lt:
        case UCD_CATEGORY_Lm:
        case UCD_CATEGORY_Lo:
        case UCD_CATEGORY_Nl: return true;

        default: return false;
    }
}

static bool
is_identifier_continue(dai_rune_t rune) {
    // Lu, Ll, Lt, Lm, Lo, Nl, the underscore
    // Mn, Mc, Nd, Pc
    if (rune == '_') {
        return true;
    }
    ucd_category category = ucd_lookup_category(rune);
    switch (category) {
        case UCD_CATEGORY_Lu:
        case UCD_CATEGORY_Ll:
        case UCD_CATEGORY_Lt:
        case UCD_CATEGORY_Lm:
        case UCD_CATEGORY_Lo:
        case UCD_CATEGORY_Nl:
        case UCD_CATEGORY_Mn:
        case UCD_CATEGORY_Mc:
        case UCD_CATEGORY_Nd:
        case UCD_CATEGORY_Pc: return true;
        default: return false;
    }
}

static DaiToken*
Tokenizer_readIdentifier(Tokenizer* tker) {
    // identifier 允许的字符
    // https://docs.python.org/3/reference/lexical_analysis.html#identifiers
    while (is_identifier_continue(tker->ch)) {
        Tokenizer_readChar(tker);
    }
    DaiToken* tok = Tokenizer_buildToken(tker, DaiTokenType_ident);
    tok->type     = lookup_ident(tok->literal);
    return tok;
}

static DaiToken*
Tokenizer_readComment(Tokenizer* tker) {
    while (tker->ch != '\n' && tker->ch != 0) {
        Tokenizer_readChar(tker);
    }
    return Tokenizer_buildToken(tker, DaiTokenType_comment);
}

static DaiToken*
Tokenizer_readString(Tokenizer* tker, const dai_rune_t quote) {
    bool multiline = quote == '`';
    while (tker->ch != quote && tker->ch != 0 && (tker->ch != '\n' || multiline)) {
        // TODO 处理转义字符
        Tokenizer_readChar(tker);
    }
    if (tker->ch != quote) {
        tker->has_error_msg = true;
        snprintf(tker->error_msg, sizeof(tker->error_msg), "unclosed string literal");
        Tokenizer_mark(tker);
        return Tokenizer_buildToken(tker, DaiTokenType_illegal);
    } else {
        Tokenizer_readChar(tker);
        return Tokenizer_buildToken(tker, DaiTokenType_str);
    }
}

static DaiToken*
Tokenizer_nextToken(Tokenizer* tker) {
    Tokenizer_skipWhitespace(tker);
    Tokenizer_mark(tker);
    dai_rune_t ch = tker->ch;
    switch (ch) {
        case '.':
        case ',':
        case ';':
        case '{':
        case '}':
        case '(':
        case ')':
        case '+':
        case '-':
        case '*':
        case '%':
        case '/': {
            if (ch == '/' && Tokenizer_peekChar(tker) == '/') {
                return Tokenizer_readComment(tker);
            } else {
                Tokenizer_readChar(tker);
                return Tokenizer_buildToken(tker, DaiTokenType_auto);
            }
        }
        case '!': {
            if (Tokenizer_peekChar(tker) == '=') {
                Tokenizer_readChar(tker);
                Tokenizer_readChar(tker);
                return Tokenizer_buildToken(tker, DaiTokenType_not_eq);
            } else {
                Tokenizer_readChar(tker);
                return Tokenizer_buildToken(tker, DaiTokenType_bang);
            }
        }
        case '=': {
            if (Tokenizer_peekChar(tker) == '=') {
                Tokenizer_readChar(tker);
                Tokenizer_readChar(tker);
                return Tokenizer_buildToken(tker, DaiTokenType_eq);
            } else {
                Tokenizer_readChar(tker);
                return Tokenizer_buildToken(tker, DaiTokenType_assign);
            }
        }
        case '<': {
            Tokenizer_readChar(tker);
            return Tokenizer_buildToken(tker, DaiTokenType_lt);
        }
        case '>': {
            Tokenizer_readChar(tker);
            return Tokenizer_buildToken(tker, DaiTokenType_gt);
        }
        case '#': {
            return Tokenizer_readComment(tker);
        }
        case '`':
        case '\'':
        case '"': {
            Tokenizer_readChar(tker);
            return Tokenizer_readString(tker, ch);
        }
        case 0: {
            return Tokenizer_buildToken(tker, DaiTokenType_eof);
        }
        default: {
            if (is_identifier_start(ch)) {
                return Tokenizer_readIdentifier(tker);
            } else if (is_digit(ch)) {
                return Tokenizer_readNumber(tker);
            } else {
                Tokenizer_readChar(tker);
                return Tokenizer_buildToken(tker, DaiTokenType_illegal);
            }
        }
    }
}
// #endregion

static DaiSyntaxError*
Tokenizer_run(Tokenizer* tker, DaiTokenList* tlist) {
    while (true) {
        size_t prev_position = tker->position;
        DaiToken* tok        = Tokenizer_nextToken(tker);
        switch (tok->type) {
            case DaiTokenType_illegal: {
                if (!tker->has_error_msg) {
                    snprintf(tker->error_msg,
                             sizeof(tker->error_msg),
                             "illegal character '%s'",
                             tok->literal);
                }
                {
                    dai_move(tker->tokens, tlist->tokens);
                    tlist->length = tker->tokens_offset;
                }
                return DaiSyntaxError_New(tker->error_msg, tok->start_line, tok->start_column);
            }
            case DaiTokenType_eof: {
                {
                    dai_move(tker->tokens, tlist->tokens);
                    tlist->length = tker->tokens_offset;
                }
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
    Tokenizer* tker     = Tokenizer_New(s);
    DaiSyntaxError* err = Tokenizer_run(tker, tlist);
    Tokenizer_free(tker);
    return err;
}