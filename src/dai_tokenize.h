#ifndef CBDAI_TOKENIZE_H
#define CBDAI_TOKENIZE_H

#include <stdbool.h>
#include <stddef.h>

#include "dai_error.h"

// Token 类型
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
    DaiTokenType_con,        // "con"
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
    DaiTokenType_auto,          // "auto"
    DaiTokenType_assign,        // "assign="
    DaiTokenType_plus,          // "plus+"
    DaiTokenType_minus,         // "minus-"
    DaiTokenType_bang,          // "bang!"
    DaiTokenType_asterisk,      // "asterisk*"
    DaiTokenType_slash,         // "slash/"
    DaiTokenType_percent,       // "percent%"
    DaiTokenType_left_shift,    // "left_shift<<"
    DaiTokenType_right_shift,   // "right_shift>>"
    DaiTokenType_bitwise_and,   // "bitwise_and&"
    DaiTokenType_bitwise_xor,   // "bitwise_xor^"
    DaiTokenType_bitwise_or,    // "bitwise_or|"
    DaiTokenType_bitwise_not,   // "bitwise_not~"

    DaiTokenType_lt,       // "lt<"
    DaiTokenType_gt,       // "gt>"
    DaiTokenType_lte,      // "lte<="
    DaiTokenType_gte,      // "gte>="
    DaiTokenType_eq,       // "eq=="
    DaiTokenType_not_eq,   // "not_eq!="

    DaiTokenType_dot,   // "dot."

    // 分隔符
    DaiTokenType_comma,       // "comma,"
    DaiTokenType_semicolon,   // "semicolon;"
    DaiTokenType_colon,       // "colon:"

    DaiTokenType_lparen,     // "lparen("
    DaiTokenType_rparen,     // "rparen)"
    DaiTokenType_lbracket,   // "lbracket["
    DaiTokenType_rbracket,   // "rbracket]"
    DaiTokenType_lbrace,     // "lbrace{"
    DaiTokenType_rbrace,     // "rbrace}"

    DaiTokenType_add_assign,   // "add_assign+="
    DaiTokenType_sub_assign,   // "sub_assign-="
    DaiTokenType_mul_assign,   // "mul_assign*="
    DaiTokenType_div_assign,   // "div_assign/="
    // #endregion

    // 结束标记
    DaiTokenType_end,
} DaiTokenType;

// 返回 Token 类型的字符串表示
__attribute__((unused)) const char*
DaiTokenType_string(const DaiTokenType type);

// Token 结构体，表示一个词法单元
// 词法单元的类型，字符串，起始行列，结束行列，长度
typedef struct {
    DaiTokenType type;
    // s + length 表示了在源文本上的一段字符串
    const char* s;
    // 从 1 开始的行和列
    int start_line;
    int start_column;
    int end_line;
    int end_column;
    size_t length;
} DaiToken;

// #region DaiTokenList 结构体及其方法，保存词法分析的结果，提供方法读取
// DaiTokenList 结构体，表示一个 Token 列表
typedef struct {
    size_t index;
    size_t length;
    DaiToken* tokens;
} DaiTokenList;
// 初始化 DaiTokenList 结构体
void
DaiTokenList_init(DaiTokenList* list);
// 重置 DaiTokenList 结构体，释放内存
void
DaiTokenList_reset(DaiTokenList* list);
// 返回当前 token 并将读取位置加一
DaiToken*
DaiTokenList_next(DaiTokenList* list);
// 返回 token 列表的长度
size_t
DaiTokenList_length(const DaiTokenList* list);
// 返回当前 token 的索引
size_t
DaiTokenList_current_index(const DaiTokenList* list);
// 返回指定索引的 token
DaiToken*
DaiTokenList_get(const DaiTokenList* list, size_t index);

// #endregion

DaiSyntaxError*
dai_tokenize_string(const char* s, DaiTokenList* tlist);

#endif   // CBDAI_TOKENIZE_H
