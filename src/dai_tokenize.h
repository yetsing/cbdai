//
// Created by  on 2024/5/28.
//

#ifndef CBDAI_TOKENIZE_H
#define CBDAI_TOKENIZE_H

#include <stdbool.h>
#include <stddef.h>

#include "dai_error.h"

typedef enum {
    DaiTokenType_illegal = 0,   // "illegal"
    DaiTokenType_eof,           // "eof"
    DaiTokenType_ident,         // "ident"
    DaiTokenType_int,           // "int"
    DaiTokenType_float,         // "float"
    DaiTokenType_comment,       // "comment"
    DaiTokenType_str,           // "string"

    // #region 关键字
    DaiTokenType_function,   // "function"
    DaiTokenType_var,        // "var"
    DaiTokenType_true,       // "true"
    DaiTokenType_false,      // "false"
    DaiTokenType_nil,        // "nil"
    DaiTokenType_if,         // "if"
    DaiTokenType_elif,       // "elif"
    DaiTokenType_else,       // "else"
    DaiTokenType_return,     // "return"
    DaiTokenType_class,      // "class"
    DaiTokenType_self,       // "self"
    DaiTokenType_super,      // "super"
    DaiTokenType_for,        // "for"
    DaiTokenType_in,         // "in"
    DaiTokenType_while,      // "while"
    DaiTokenType_break,      // "break"
    DaiTokenType_continue,   // "continue"
    DaiTokenType_and,        // "and"
    DaiTokenType_or,         // "or"
    DaiTokenType_not,        // "not"
    // #endregion

    // #region 符号，自动类型，会根据字面量自动转换成对应类型
    //  在他下面的都满足这个要求
    DaiTokenType_auto,       // "auto"
    DaiTokenType_assign,     // "assign="
    DaiTokenType_plus,       // "plus+"
    DaiTokenType_minus,      // "minus-"
    DaiTokenType_bang,       // "bang!"
    DaiTokenType_asterisk,   // "asterisk*"
    DaiTokenType_slash,      // "slash/"
    DaiTokenType_percent,    // "percent%"

    DaiTokenType_lt,    // "lt<"
    DaiTokenType_gt,    // "gt>"
    DaiTokenType_lte,   // "lte<="
    DaiTokenType_gte,   // "gte>="

    DaiTokenType_eq,       // "eq=="
    DaiTokenType_not_eq,   // "not_eq!="

    DaiTokenType_dot,   // "dot."

    // 分隔符
    DaiTokenType_comma,       // "comma,"
    DaiTokenType_semicolon,   // "semicolon;"

    DaiTokenType_lparen,   // "lparen("
    DaiTokenType_rparen,   // "rparen)"
    DaiTokenType_lbrace,   // "lbrace{"
    DaiTokenType_rbrace,   // "rbrace}"
    // #endregion

    // 结束标记
    DaiTokenType_end,
} DaiTokenType;

__attribute__((unused)) const char*
DaiTokenType_string(const DaiTokenType type);

typedef struct {
    DaiTokenType type;
    char* literal;
    // 从 1 开始的行和列
    int start_line;
    int start_column;
    int end_line;
    int end_column;
} DaiToken;

// #region DaiTokenList 结构体及其方法，保存词法分析的结果和错误，提供方法读取
typedef struct _DaiTokenList {
    size_t index;
    size_t length;
    DaiToken* tokens;
} DaiTokenList;

DaiTokenList*
DaiTokenList_New();
void
DaiTokenList_free(DaiTokenList* list);
void
DaiTokenList_init(DaiTokenList* list);
void
DaiTokenList_reset(DaiTokenList* list);
// 返回当前 token 并将读取位置加一
DaiToken*
DaiTokenList_next(DaiTokenList* list);
size_t
DaiTokenList_length(const DaiTokenList* list);

// #endregion

DaiSyntaxError*
dai_tokenize_string(const char* s, DaiTokenList* tlist);

#endif   // CBDAI_TOKENIZE_H
