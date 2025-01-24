#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "dai_assert.h"
#include "dai_ast.h"
#include "dai_common.h"
#include "dai_malloc.h"
#include "dai_parse.h"
#include "dai_parseint.h"
#include "dai_tokenize.h"

typedef struct TParser Parser;

// 解析函数需要遵循一个约定
// 函数在开始解析表达式时， cur_token 必须是当前所关联的 token 类型，
// 返回分析的表达式结果时， cur_token 是当前表达式类型中的最后一个 token
// 比如 5 + 4 解析
// 开始解析时， cur_token = 5
// 返回分析的表达式结果时， cur_token = 4
// 前缀表达式解析函数
typedef DaiAstExpression* (*prefixParseFn)(Parser* p);
// 中缀表达式解析函数
typedef DaiAstExpression* (*infixParseFn)(Parser* p, DaiAstExpression* left);

// 表达式优先级
typedef enum {
    Precedence_none = 0,
    // 优先级最低
    Precedence_Lowest,
    Precedence_Or,            // or
    Precedence_And,           // and
    Precedence_Not,           // not
    Precedence_Compare,       // <, <=, >, >=, !=, ==
    Precedence_Bitwise_Or,    // |
    Precedence_Bitwise_Xor,   // ^
    Precedence_Bitwise_And,   // &
    Precedence_Shift,         // <<, >>
    Precedence_Sum,           // +, -
    Precedence_Product,       // *, /, %
    Precedence_Prefix,        // -X, !X, ~X
    Precedence_Call,          // myFunction(X)
    // 优先级最高
    Precedence_Highest,

} Precedence;

static Precedence
token_precedences(const DaiTokenType type) {
    switch (type) {
        case DaiTokenType_or: return Precedence_Or;
        case DaiTokenType_and: return Precedence_And;
        case DaiTokenType_not: return Precedence_Not;
        case DaiTokenType_eq:
        case DaiTokenType_not_eq:
        case DaiTokenType_lte:
        case DaiTokenType_gte:
        case DaiTokenType_lt:
        case DaiTokenType_gt: return Precedence_Compare;
        case DaiTokenType_bitwise_or: return Precedence_Bitwise_Or;
        case DaiTokenType_bitwise_xor: return Precedence_Bitwise_Xor;
        case DaiTokenType_bitwise_and: return Precedence_Bitwise_And;
        case DaiTokenType_right_shift:
        case DaiTokenType_left_shift: return Precedence_Shift;
        case DaiTokenType_minus:
        case DaiTokenType_plus: return Precedence_Sum;
        case DaiTokenType_percent:
        case DaiTokenType_slash:
        case DaiTokenType_asterisk: return Precedence_Product;
        case DaiTokenType_lbracket:
        case DaiTokenType_lparen:
        case DaiTokenType_dot: return Precedence_Call;
        default: return Precedence_Lowest;
    }
}

typedef struct TParser {
    DaiTokenList* tlist;

    // 上一个 token ，帮助展示更准确的错误信息
    DaiToken* prev_token;
    // 当前 token
    DaiToken* cur_token;
    // 下一个 token
    DaiToken* peek_token;

    // DaiTokenType 对应前缀、中缀解析函数
    prefixParseFn prefix_parse_fns[DaiTokenType_end];
    infixParseFn infix_parse_fns[DaiTokenType_end];

    // syntax error 需要返回给调用方，所以不应该由 parser 释放其内存
    DaiSyntaxError* syntax_error;
} Parser;

// #region parser 辅助方法声明

static Parser*
Parser_New(DaiTokenList* tlist);
static void
Parser_free(Parser* p);
static void
Parser_nextToken(Parser* p);
static bool
Parser_curTokenIs(const Parser* p, DaiTokenType type);
static bool
Parser_peekTokenIs(const Parser* p, DaiTokenType type);
static bool
Parser_expectPeek(Parser* p, DaiTokenType type);

static Precedence
Parser_curPrecedence(const Parser* p);
static Precedence
Parser_peekPrecedence(const Parser* p);

static void
Parser_register(Parser* p);
static void
Parser_registerPrefix(Parser* p, DaiTokenType type, prefixParseFn fn);
static void
Parser_registerInfix(Parser* p, DaiTokenType type, infixParseFn fn);
// #endregion

// #region parser 解析方法声明

static DaiAstExpression*
Parser_parseInteger(Parser* p);
static DaiAstExpression*
Parser_parseFloat(Parser* p);
static DaiAstExpression*
Parser_parseIdentifier(Parser* p);
static DaiAstExpression*
Parser_parseBoolean(Parser* p);
static DaiAstExpression*
Parser_parseNil(Parser* p);
static DaiAstExpression*
Parser_parsePrefixExpression(Parser* p);
static DaiAstExpression*
Parser_parseInfixExpression(Parser* p, DaiAstExpression* left);
static DaiAstExpression*
Parser_parseGroupedExpression(Parser* p);
static DaiAstExpression*
Parser_parseCallExpression(Parser* p, DaiAstExpression* left);
static DaiAstExpression*
Parser_parseDotExpression(Parser* p, DaiAstExpression* left);
static DaiAstExpression*
Parser_parseSelfExpression(Parser* p);
static DaiAstExpression*
Parser_parseSuperExpression(Parser* p);
static DaiAstExpression*
Parser_parseFunctionLiteral(Parser* p);
static DaiAstExpression*
Parser_parseStringLiteral(Parser* p);
static DaiAstExpression*
Parser_parseArrayLiteral(Parser* p);
static DaiAstExpression*
Parser_parseMapLiteral(Parser* p);
static DaiAstExpression*
Parser_parseSubscriptExpression(Parser* p, DaiAstExpression* left);
static DaiAstExpression*
Parser_parseExpression(Parser* p, Precedence precedence);

static DaiAstStatement*
Parser_parseStatement(Parser* p);
static DaiAstBlockStatement*
Parser_parseBlockStatement(Parser* p);
static DaiAstBlockStatement*
Parser_parseBlockStatement1(Parser* p);
static DaiAstStatement*
Parser_parseStatementOfClass(Parser* p);
// #endregion

// #region parser method
static Parser*
Parser_New(DaiTokenList* tlist) {
    Parser* p       = dai_malloc(sizeof(Parser));
    p->tlist        = tlist;
    p->prev_token   = NULL;
    p->cur_token    = NULL;
    p->peek_token   = NULL;
    p->syntax_error = NULL;

    // 读取两个 token ，以设置 cur_token 和 peek_token
    Parser_nextToken(p);
    Parser_nextToken(p);
    assert(p->cur_token && p->peek_token);

    Parser_register(p);
    return p;
}

// 读取下一个 token
static void
Parser_nextToken(Parser* p) {
    p->prev_token = p->cur_token;
    p->cur_token  = p->peek_token;
    p->peek_token = DaiTokenList_next(p->tlist);
    // 跳过注释
    while (p->cur_token && p->cur_token->type == DaiTokenType_comment) {
        p->prev_token = p->cur_token;
        p->cur_token  = p->peek_token;
        p->peek_token = DaiTokenList_next(p->tlist);
    }
    while (p->peek_token && p->peek_token->type == DaiTokenType_comment) {
        p->peek_token = DaiTokenList_next(p->tlist);
    }
}

// 判断当前 token 是否为期望的 token 类型
static bool
Parser_curTokenIs(const Parser* p, const DaiTokenType type) {
    return p->cur_token->type == type;
}

// 判断下一个 token 是否为期望的 token 类型
static bool
Parser_peekTokenIs(const Parser* p, const DaiTokenType type) {
    return p->peek_token->type == type;
}

static bool
Parser_peekTokenIn(const Parser* p, ...) {
    va_list args;
    va_start(args, p);
    DaiTokenType type;
    while ((type = va_arg(args, DaiTokenType)) != DaiTokenType_end) {
        if (p->peek_token->type == type) {
            va_end(args);
            return true;
        }
    }
    va_end(args);
    return false;
}

// 检查下一个 token 是否为期望的 token 类型
// 如果是，则读取下一个 token
// 否则设置 syntax_error 并返回 false
static bool
Parser_expectPeek(Parser* p, const DaiTokenType type) {
    if (Parser_peekTokenIs(p, type)) {
        Parser_nextToken(p);
        return true;
    }
    char buf[128];
    snprintf(buf,
             128,
             "expected token to be \"%s\" but got \"%s\"",
             DaiTokenType_string(type),
             DaiTokenType_string(p->peek_token->type));
    int line        = p->peek_token->start_line;
    int column      = p->peek_token->start_column;
    p->syntax_error = DaiSyntaxError_New(buf, line, column);
    return false;
}

// 获取当前 token 的优先级
static Precedence
Parser_curPrecedence(const Parser* p) {
    return token_precedences(p->cur_token->type);
}

// 获取下一个 token 的优先级
static Precedence
Parser_peekPrecedence(const Parser* p) {
    return token_precedences(p->peek_token->type);
}

// 注册前缀表达式解析函数
static void
Parser_registerPrefix(Parser* p, DaiTokenType type, prefixParseFn fn) {
    p->prefix_parse_fns[type] = fn;
}

// 注册中缀表达式解析函数
static void
Parser_registerInfix(Parser* p, DaiTokenType type, infixParseFn fn) {
    p->infix_parse_fns[type] = fn;
}

// 释放解析器
static void
Parser_free(Parser* p) {
    dai_free(p);
}

static void
Parser_register(Parser* p) {
    // 初始化
    for (int i = 0; i < DaiTokenType_end; i++) {
        p->prefix_parse_fns[i] = NULL;
        p->infix_parse_fns[i]  = NULL;
    }
    // 注册前缀表达式解析函数
    {
        Parser_registerPrefix(p, DaiTokenType_ident, Parser_parseIdentifier);
        Parser_registerPrefix(p, DaiTokenType_int, Parser_parseInteger);
        Parser_registerPrefix(p, DaiTokenType_float, Parser_parseFloat);
        Parser_registerPrefix(p, DaiTokenType_bang, Parser_parsePrefixExpression);
        Parser_registerPrefix(p, DaiTokenType_minus, Parser_parsePrefixExpression);
        Parser_registerPrefix(p, DaiTokenType_not, Parser_parsePrefixExpression);
        Parser_registerPrefix(p, DaiTokenType_bitwise_not, Parser_parsePrefixExpression);
        Parser_registerPrefix(p, DaiTokenType_true, Parser_parseBoolean);
        Parser_registerPrefix(p, DaiTokenType_false, Parser_parseBoolean);
        Parser_registerPrefix(p, DaiTokenType_nil, Parser_parseNil);
        Parser_registerPrefix(p, DaiTokenType_lparen, Parser_parseGroupedExpression);
        Parser_registerPrefix(p, DaiTokenType_function, Parser_parseFunctionLiteral);
        Parser_registerPrefix(p, DaiTokenType_str, Parser_parseStringLiteral);
        Parser_registerPrefix(p, DaiTokenType_self, Parser_parseSelfExpression);
        Parser_registerPrefix(p, DaiTokenType_super, Parser_parseSuperExpression);
        Parser_registerPrefix(p, DaiTokenType_lbracket, Parser_parseArrayLiteral);
        Parser_registerPrefix(p, DaiTokenType_lbrace, Parser_parseMapLiteral);
    }

    // 注册中缀表达式解析函数
    {
        Parser_registerInfix(p, DaiTokenType_bitwise_and, Parser_parseInfixExpression);
        Parser_registerInfix(p, DaiTokenType_bitwise_xor, Parser_parseInfixExpression);
        Parser_registerInfix(p, DaiTokenType_bitwise_or, Parser_parseInfixExpression);
        Parser_registerInfix(p, DaiTokenType_left_shift, Parser_parseInfixExpression);
        Parser_registerInfix(p, DaiTokenType_right_shift, Parser_parseInfixExpression);
        Parser_registerInfix(p, DaiTokenType_plus, Parser_parseInfixExpression);
        Parser_registerInfix(p, DaiTokenType_minus, Parser_parseInfixExpression);
        Parser_registerInfix(p, DaiTokenType_slash, Parser_parseInfixExpression);
        Parser_registerInfix(p, DaiTokenType_asterisk, Parser_parseInfixExpression);
        Parser_registerInfix(p, DaiTokenType_percent, Parser_parseInfixExpression);
        Parser_registerInfix(p, DaiTokenType_eq, Parser_parseInfixExpression);
        Parser_registerInfix(p, DaiTokenType_not_eq, Parser_parseInfixExpression);
        Parser_registerInfix(p, DaiTokenType_lt, Parser_parseInfixExpression);
        Parser_registerInfix(p, DaiTokenType_gt, Parser_parseInfixExpression);
        Parser_registerInfix(p, DaiTokenType_lte, Parser_parseInfixExpression);
        Parser_registerInfix(p, DaiTokenType_gte, Parser_parseInfixExpression);
        Parser_registerInfix(p, DaiTokenType_and, Parser_parseInfixExpression);
        Parser_registerInfix(p, DaiTokenType_or, Parser_parseInfixExpression);
        Parser_registerInfix(p, DaiTokenType_lparen, Parser_parseCallExpression);
        Parser_registerInfix(p, DaiTokenType_dot, Parser_parseDotExpression);
        Parser_registerInfix(p, DaiTokenType_lbracket, Parser_parseSubscriptExpression);
    }
}

// #endregion

// #region 解析表达式

static void
Parser_FreeExpressionList(DaiAstExpression** list, size_t count) {
    for (size_t i = 0; i < count; i++) {
        list[i]->free_fn((DaiAstBase*)list[i], true);
    }
    dai_free(list);
}

// 返回 NULL 表示解析失败
static DaiAstExpression**
Parser_parseExpressionList(Parser* p, const DaiTokenType end, size_t* arg_count) {
    size_t arg_size         = 4;
    DaiAstExpression** args = (DaiAstExpression**)dai_malloc(sizeof(DaiAstExpression*) * arg_size);
    *arg_count              = 0;

    // 没有调用参数
    if (Parser_peekTokenIs(p, end)) {
        Parser_nextToken(p);
        *arg_count = 0;
        return args;
    }
    // 跳过 ([ 括号 token
    Parser_nextToken(p);
    DaiAstExpression* arg = Parser_parseExpression(p, Precedence_Lowest);
    if (arg == NULL) {
        Parser_FreeExpressionList(args, *arg_count);
        return NULL;
    }
    args[*arg_count] = arg;
    (*arg_count)++;
    while (Parser_peekTokenIs(p, DaiTokenType_comma)) {
        {
            // 跳过 , 逗号 token
            Parser_nextToken(p);
            if (Parser_peekTokenIs(p, end)) {
                // 支持参数末尾多余的逗号，例如 (a, b,) b 后面的逗号
                break;
            }
            // 再 next 一次，让当前 token 是表达式的开始
            Parser_nextToken(p);
        }
        {
            if (*arg_count == arg_size) {
                arg_size *= 2;
                args = (DaiAstExpression**)dai_realloc(args, sizeof(DaiAstExpression*) * arg_size);
            }
        }
        arg = Parser_parseExpression(p, Precedence_Lowest);
        if (arg == NULL) {
            Parser_FreeExpressionList(args, *arg_count);
            return NULL;
        }
        args[*arg_count] = arg;
        (*arg_count)++;
    }
    // 结尾的 )] 括号 token
    if (!Parser_expectPeek(p, end)) {
        Parser_FreeExpressionList(args, *arg_count);
        return NULL;
    }
    return args;
}

DaiAstExpression*
Parser_parseArrayLiteral(Parser* p) {
    DaiAstArrayLiteral* array = DaiAstArrayLiteral_New();
    {
        array->start_line   = p->cur_token->start_line;
        array->start_column = p->cur_token->start_column;
    }
    array->elements = Parser_parseExpressionList(p, DaiTokenType_rbracket, &array->length);
    if (array->elements == NULL) {
        array->free_fn((DaiAstBase*)array, true);
        return NULL;
    }
    {
        array->end_line   = p->cur_token->end_line;
        array->end_column = p->cur_token->end_column;
    }
    return (DaiAstExpression*)array;
}

static void
Parser_FreeMapPairList(DaiAstMapLiteralPair* list, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (list[i].key) {
            list[i].key->free_fn((DaiAstBase*)list[i].key, true);
        }
        if (list[i].value) {
            list[i].value->free_fn((DaiAstBase*)list[i].value, true);
        }
    }
    dai_free(list);
}

// 返回 NULL 表示解析失败
static DaiAstMapLiteralPair*
Parser_parseMapPairList(Parser* p, const DaiTokenType end, size_t* pair_count) {
    size_t size = 4;
    DaiAstMapLiteralPair* pairs =
        (DaiAstMapLiteralPair*)dai_malloc(sizeof(DaiAstMapLiteralPair) * size);
    memset(pairs, 0, sizeof(DaiAstMapLiteralPair) * size);
    *pair_count = 0;

    // 没有调用参数
    if (Parser_peekTokenIs(p, end)) {
        Parser_nextToken(p);
        *pair_count = 0;
        return pairs;
    }
    // 跳过 { 括号 token
    Parser_nextToken(p);
    DaiAstExpression* key = Parser_parseExpression(p, Precedence_Lowest);
    if (key == NULL) {
        Parser_FreeMapPairList(pairs, *pair_count);
        return NULL;
    }
    pairs[*pair_count].key = key;
    if (!Parser_expectPeek(p, DaiTokenType_colon)) {
        Parser_FreeMapPairList(pairs, *pair_count);
        return NULL;
    }
    // 跳过 : 冒号 token
    Parser_nextToken(p);
    DaiAstExpression* val = Parser_parseExpression(p, Precedence_Lowest);
    if (val == NULL) {
        Parser_FreeMapPairList(pairs, *pair_count);
        return NULL;
    }
    pairs[*pair_count].value = val;
    (*pair_count)++;
    while (Parser_peekTokenIs(p, DaiTokenType_comma)) {
        {
            // 跳过 , 逗号 token
            Parser_nextToken(p);
            if (Parser_peekTokenIs(p, end)) {
                // 支持参数末尾多余的逗号，例如 (a, b,) b 后面的逗号
                break;
            }
            // 再 next 一次，让当前 token 是表达式的开始
            Parser_nextToken(p);
        }
        {
            // 扩容
            if (*pair_count == size) {
                size *= 2;
                pairs =
                    (DaiAstMapLiteralPair*)dai_realloc(pairs, sizeof(DaiAstMapLiteralPair) * size);
            }
        }
        DaiAstExpression* key = Parser_parseExpression(p, Precedence_Lowest);
        if (key == NULL) {
            Parser_FreeMapPairList(pairs, *pair_count);
            return NULL;
        }
        pairs[*pair_count].key = key;
        if (!Parser_expectPeek(p, DaiTokenType_colon)) {
            Parser_FreeMapPairList(pairs, *pair_count);
            return NULL;
        }
        // 跳过 : 冒号 token
        Parser_nextToken(p);
        DaiAstExpression* val = Parser_parseExpression(p, Precedence_Lowest);
        if (val == NULL) {
            Parser_FreeMapPairList(pairs, *pair_count);
            return NULL;
        }
        pairs[*pair_count].value = val;
        (*pair_count)++;
    }
    // 结尾的 } 括号 token
    if (!Parser_expectPeek(p, end)) {
        Parser_FreeMapPairList(pairs, *pair_count);
        return NULL;
    }
    return pairs;
}

DaiAstExpression*
Parser_parseMapLiteral(Parser* p) {
    DaiAstMapLiteral* map = DaiAstMapLiteral_New();
    {
        map->start_line   = p->cur_token->start_line;
        map->start_column = p->cur_token->start_column;
    }
    map->pairs = Parser_parseMapPairList(p, DaiTokenType_rbrace, &map->length);
    if (map->pairs == NULL) {
        map->free_fn((DaiAstBase*)map, true);
        return NULL;
    }
    {
        map->end_line   = p->cur_token->end_line;
        map->end_column = p->cur_token->end_column;
    }
    return (DaiAstExpression*)map;
}

static DaiAstExpression*
Parser_parseBoolean(Parser* p) {
    daiassert(p->cur_token->type == DaiTokenType_true || p->cur_token->type == DaiTokenType_false,
              "not a boolean: %s",
              DaiTokenType_string(p->cur_token->type));
    return (DaiAstExpression*)DaiAstBoolean_New(p->cur_token);
}

// 解析整数字面量
static DaiAstExpression*
Parser_parseInteger(Parser* p) {
    daiassert(p->cur_token->type == DaiTokenType_int,
              "not an integer: %s",
              DaiTokenType_string(p->cur_token->type));
    // 解析字符串为整数
    int base      = 10;
    char* literal = p->cur_token->literal;
    if (strlen(literal) >= 3 && literal[0] == '0') {
        switch (literal[1]) {
            case 'x':
            case 'X': base = 16; break;
            case 'o':
            case 'O': base = 8; break;
            case 'b':
            case 'B': base = 2; break;
        }
    }
    char* error = NULL;
    int64_t n   = dai_parseint(literal, base, &error);
    if (error != NULL) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%s \"%s\"", error, p->cur_token->literal);
        int line        = p->cur_token->start_line;
        int column      = p->cur_token->start_column;
        p->syntax_error = DaiSyntaxError_New(buf, line, column);
        return NULL;
    }
    // 创建整数节点
    DaiAstIntegerLiteral* num = DaiAstIntegerLiteral_New(p->cur_token);
    num->value                = n;
    return (DaiAstExpression*)num;
}

// 解析浮点数字面量
static DaiAstExpression*
Parser_parseFloat(Parser* p) {
    daiassert(p->cur_token->type == DaiTokenType_float,
              "not an float: %s",
              DaiTokenType_string(p->cur_token->type));

    // 去除字面量中的下划线
    char* numbuf      = dai_malloc(strlen(p->cur_token->literal) + 1);
    char* numbuf_curr = numbuf;
    char* curr        = p->cur_token->literal;
    while (*curr != '\0') {
        if (*curr == '_') {
            curr++;
        } else {
            *numbuf_curr++ = *curr++;
        }
    }
    *numbuf_curr = '\0';

    char* end;
    const double value = strtod(numbuf, &end);
    dai_free(numbuf);
    if (errno == ERANGE) {
        char buf[256];
        snprintf(buf, sizeof(buf), "Out of range \"%s\"", p->cur_token->literal);
        int line        = p->cur_token->start_line;
        int column      = p->cur_token->start_column;
        p->syntax_error = DaiSyntaxError_New(buf, line, column);
        return NULL;
    }
    DaiAstFloatLiteral* num = DaiAstFloatLiteral_New(p->cur_token);
    num->value              = value;
    return (DaiAstExpression*)num;
}

static DaiAstExpression*
Parser_parseNil(Parser* p) {
    daiassert(p->cur_token->type == DaiTokenType_nil,
              "not a nil: %s",
              DaiTokenType_string(p->cur_token->type));
    return (DaiAstExpression*)DaiAstNil_New(p->cur_token);
}

static DaiAstExpression*
Parser_parseStringLiteral(Parser* p) {
    return (DaiAstExpression*)DaiAstStringLiteral_New(p->cur_token);
}

// 解析标识符
static DaiAstExpression*
Parser_parseIdentifier(Parser* p) {
    daiassert(p->cur_token->type == DaiTokenType_ident,
              "not an identifier: %s",
              DaiTokenType_string(p->cur_token->type));
    return (DaiAstExpression*)DaiAstIdentifier_New(p->cur_token);
}

static void
Parser_FreeFunctionParameters(DaiAstIdentifier** params, size_t param_count) {
    for (size_t i = 0; i < param_count; i++) {
        params[i]->free_fn((DaiAstBase*)params[i], true);
    }
    dai_free(params);
}

// 返回 NULL 表示解析失败
static DaiAstIdentifier**
Parser_parseFunctionParameters(Parser* p, size_t* param_count) {
    size_t param_size         = 4;
    DaiAstIdentifier** params = dai_malloc(sizeof(DaiAstIdentifier*) * param_size);
    *param_count              = 0;

    // 没有参数
    if (Parser_peekTokenIs(p, DaiTokenType_rparen)) {
        Parser_nextToken(p);
        *param_count = 0;
        return params;
    }

    // 第一个参数
    if (!Parser_expectPeek(p, DaiTokenType_ident)) {
        Parser_FreeFunctionParameters(params, *param_count);
        return NULL;
    }
    DaiAstIdentifier* param = DaiAstIdentifier_New(p->cur_token);
    params[*param_count]    = param;
    (*param_count)++;
    // 解析后面的参数，格式为 (, ident) ...
    while (Parser_peekTokenIs(p, DaiTokenType_comma)) {
        Parser_nextToken(p);
        if (Parser_peekTokenIs(p, DaiTokenType_rparen)) {
            // 支持参数末尾多余的逗号，例如 (a, b,) ，b 后面的逗号
            break;
        }
        if (!Parser_expectPeek(p, DaiTokenType_ident)) {
            Parser_FreeFunctionParameters(params, *param_count);
            return NULL;
        }
        // 扩容
        {
            if (*param_count == param_size) {
                param_size *= 2;
                params = dai_realloc(params, sizeof(DaiAstIdentifier*) * param_size);
                for (size_t i = *param_count; i < param_size; i++) {
                    params[i] = NULL;
                }
            }
        }

        param                = DaiAstIdentifier_New(p->cur_token);
        params[*param_count] = param;
        (*param_count)++;
    }
    // 结尾右括号
    if (!Parser_expectPeek(p, DaiTokenType_rparen)) {
        Parser_FreeFunctionParameters(params, *param_count);
        return NULL;
    }
    return params;
}

static DaiAstExpression*
Parser_parseFunctionLiteral(Parser* p) {
    DaiAstFunctionLiteral* func = DaiAstFunctionLiteral_New();
    {
        func->start_line   = p->cur_token->start_line;
        func->start_column = p->cur_token->start_column;
    }

    // 解析函数参数
    if (!Parser_expectPeek(p, DaiTokenType_lparen)) {
        func->free_fn((DaiAstBase*)func, true);
        return NULL;
    }
    func->parameters = Parser_parseFunctionParameters(p, &func->parameters_count);
    if (func->parameters == NULL) {
        func->free_fn((DaiAstBase*)func, true);
        return NULL;
    }
    // 解析函数体
    if (!Parser_expectPeek(p, DaiTokenType_lbrace)) {
        func->free_fn((DaiAstBase*)func, true);
        return NULL;
    }
    func->body = Parser_parseBlockStatement1(p);
    if (func->body == NULL) {
        func->free_fn((DaiAstBase*)func, true);
        return NULL;
    }

    {
        func->end_line   = p->cur_token->end_line;
        func->end_column = p->cur_token->end_column;
    }
    return (DaiAstExpression*)func;
}

static DaiAstExpression*
Parser_parseCallExpression(Parser* p, DaiAstExpression* left) {
    DaiAstCallExpression* call = DaiAstCallExpression_New();
    {
        call->start_line   = left->start_line;
        call->start_column = left->start_column;
    }
    call->function  = left;
    call->arguments = Parser_parseExpressionList(p, DaiTokenType_rparen, &call->arguments_count);
    if (call->arguments == NULL) {
        call->free_fn((DaiAstBase*)call, true);
        return NULL;
    }
    {
        call->end_line   = p->cur_token->end_line;
        call->end_column = p->cur_token->end_column;
    }
    return (DaiAstExpression*)call;
}

static DaiAstExpression*
Parser_parseDotExpression(Parser* p, DaiAstExpression* left) {
    DaiAstDotExpression* expr = DaiAstDotExpression_New(left);
    {
        expr->start_line   = left->start_line;
        expr->start_column = left->start_column;
    }
    if (!Parser_expectPeek(p, DaiTokenType_ident)) {
        expr->free_fn((DaiAstBase*)expr, true);
        return NULL;
    }
    DaiAstIdentifier* name = (DaiAstIdentifier*)Parser_parseIdentifier(p);
    expr->name             = name;
    {
        expr->end_line   = name->end_line;
        expr->end_column = name->end_column;
    }
    return (DaiAstExpression*)expr;
}

static DaiAstExpression*
Parser_parseSubscriptExpression(Parser* p, DaiAstExpression* left) {
    DaiAstSubscriptExpression* subscript = DaiAstSubscriptExpression_New(left);
    Parser_nextToken(p);
    subscript->right = Parser_parseExpression(p, Precedence_Lowest);
    if (subscript->right == NULL) {
        subscript->free_fn((DaiAstBase*)subscript, true);
        return NULL;
    }
    if (!Parser_expectPeek(p, DaiTokenType_rbracket)) {
        subscript->free_fn((DaiAstBase*)subscript, true);
        return NULL;
    }
    {
        subscript->end_line   = subscript->right->end_line;
        subscript->end_column = subscript->right->end_column;
    }
    return (DaiAstExpression*)subscript;
}

DaiAstExpression*
Parser_parseGroupedExpression(Parser* p) {
    Parser_nextToken(p);
    DaiAstExpression* exp = Parser_parseExpression(p, Precedence_Lowest);
    if (exp == NULL) {
        return NULL;
    }
    if (!Parser_expectPeek(p, DaiTokenType_rparen)) {
        exp->free_fn((DaiAstBase*)exp, true);
        return NULL;
    }
    return exp;
}

// 解析前缀表达式，如 "-1" "!3"
static DaiAstExpression*
Parser_parsePrefixExpression(Parser* p) {
    DaiAstPrefixExpression* prefix =
        (DaiAstPrefixExpression*)DaiAstPrefixExpression_New(p->cur_token, NULL);
    Parser_nextToken(p);
    prefix->right = Parser_parseExpression(p, Precedence_Prefix);
    if (prefix->right == NULL) {
        prefix->free_fn((DaiAstBase*)prefix, true);
        return NULL;
    }
    prefix->end_line   = prefix->right->end_line;
    prefix->end_column = prefix->right->end_column;
    return (DaiAstExpression*)prefix;
}

static DaiAstExpression*
Parser_parseInfixExpression(Parser* p, DaiAstExpression* left) {
    // 创建中缀表达式节点，此时 cur_token 是操作符
    DaiAstInfixExpression* expr = DaiAstInfixExpression_New(p->cur_token->literal, left);
    // 获取当前操作的优先级
    Precedence precedence = Parser_curPrecedence(p);
    Parser_nextToken(p);
    // 解析右侧表达式
    DaiAstExpression* right = Parser_parseExpression(p, precedence);
    if (right == NULL) {
        expr->free_fn((DaiAstBase*)expr, true);
        return NULL;
    }
    expr->right      = right;
    expr->end_line   = right->end_line;
    expr->end_column = right->end_column;
    return (DaiAstExpression*)expr;
}

static DaiAstExpression*
Parser_parseSelfExpression(Parser* p) {
    DaiAstSelfExpression* expr = DaiAstSelfExpression_New();
    {
        expr->start_line   = p->cur_token->start_line;
        expr->start_column = p->cur_token->start_column;
    }
    if (!Parser_peekTokenIs(p, DaiTokenType_dot)) {
        expr->end_line   = p->cur_token->end_line;
        expr->end_column = p->cur_token->end_column;
        return (DaiAstExpression*)expr;
    }
    if (!Parser_expectPeek(p, DaiTokenType_dot)) {
        expr->free_fn((DaiAstBase*)expr, true);
        return NULL;
    }
    if (!Parser_expectPeek(p, DaiTokenType_ident)) {
        expr->free_fn((DaiAstBase*)expr, true);
        return NULL;
    }
    DaiAstIdentifier* name = (DaiAstIdentifier*)Parser_parseIdentifier(p);
    expr->name             = name;
    {
        expr->end_line   = name->end_line;
        expr->end_column = name->end_column;
    }
    return (DaiAstExpression*)expr;
}

static DaiAstExpression*
Parser_parseSuperExpression(Parser* p) {
    DaiAstSuperExpression* expr = DaiAstSuperExpression_New();
    {
        expr->start_line   = p->cur_token->start_line;
        expr->start_column = p->cur_token->start_column;
    }
    if (!Parser_expectPeek(p, DaiTokenType_dot)) {
        expr->free_fn((DaiAstBase*)expr, true);
        return NULL;
    }
    if (!Parser_expectPeek(p, DaiTokenType_ident)) {
        expr->free_fn((DaiAstBase*)expr, true);
        return NULL;
    }
    DaiAstIdentifier* name = (DaiAstIdentifier*)Parser_parseIdentifier(p);
    expr->name             = name;
    {
        expr->end_line   = name->end_line;
        expr->end_column = name->end_column;
    }
    return (DaiAstExpression*)expr;
}

static DaiAstExpression*
Parser_parseExpression(Parser* p, Precedence precedence) {
    DaiAstExpression* left_exp = NULL;
    // 解析前缀表达式
    {
        prefixParseFn prefix_fn = p->prefix_parse_fns[p->cur_token->type];
        if (prefix_fn == NULL) {
            char buf[128];
            snprintf(buf,
                     sizeof(buf),
                     "no prefix parse function for \"%s\" found",
                     DaiTokenType_string(p->cur_token->type));
            int line        = p->cur_token->start_line;
            int column      = p->cur_token->start_column;
            p->syntax_error = DaiSyntaxError_New(buf, line, column);
            return NULL;
        }
        left_exp = prefix_fn(p);
    }
    // 解析中缀表达式
    while (!Parser_peekTokenIs(p, DaiTokenType_semicolon) &&
           precedence < Parser_peekPrecedence(p)) {
        infixParseFn infix_fn = p->infix_parse_fns[p->peek_token->type];
        assert(infix_fn != NULL);
        Parser_nextToken(p);
        left_exp = infix_fn(p, left_exp);
    }
    return left_exp;
}

// #endregion

// #region 解析语句

static DaiAstBlockStatement*
Parser_parseBlockStatementOfClass(Parser* p) {
    DaiAstBlockStatement* blockstatement = DaiAstBlockStatement_New();
    blockstatement->start_line           = p->cur_token->start_line;
    blockstatement->start_column         = p->cur_token->start_column;
    Parser_nextToken(p);

    while (!Parser_curTokenIs(p, DaiTokenType_rbrace) && !Parser_curTokenIs(p, DaiTokenType_eof)) {
        DaiAstStatement* stmt = Parser_parseStatementOfClass(p);
        if (stmt == NULL) {
            blockstatement->free_fn((DaiAstBase*)blockstatement, true);
            return NULL;
        }
        DaiAstBlockStatement_append(blockstatement, stmt);
        Parser_nextToken(p);
    }
    if (!Parser_curTokenIs(p, DaiTokenType_rbrace)) {
        int line        = p->cur_token->start_line;
        int column      = p->cur_token->start_column;
        p->syntax_error = DaiSyntaxError_New("expected '}'", line, column);
        blockstatement->free_fn((DaiAstBase*)blockstatement, true);
        return NULL;
    }
    blockstatement->end_line   = p->cur_token->end_line;
    blockstatement->end_column = p->cur_token->end_column;
    return blockstatement;
}

DaiAstClassMethodStatement*
Parser_parseClassMethodStatement(Parser* p) {
    DaiAstClassMethodStatement* func = DaiAstClassMethodStatement_New();
    {
        func->start_line   = p->cur_token->start_line;
        func->start_column = p->cur_token->start_column;
    }
    // 跳过 class 关键字，现在当前 token 是 var
    Parser_nextToken(p);


    if (!Parser_expectPeek(p, DaiTokenType_ident)) {
        func->free_fn((DaiAstBase*)func, true);
        return NULL;
    }
    dai_move(p->cur_token->literal, func->name);

    // 解析函数参数
    if (!Parser_expectPeek(p, DaiTokenType_lparen)) {
        func->free_fn((DaiAstBase*)func, true);
        return NULL;
    }
    func->parameters = Parser_parseFunctionParameters(p, &func->parameters_count);
    if (func->parameters == NULL) {
        func->free_fn((DaiAstBase*)func, true);
        return NULL;
    }
    // 解析函数体
    if (!Parser_expectPeek(p, DaiTokenType_lbrace)) {
        func->free_fn((DaiAstBase*)func, true);
        return NULL;
    }
    func->body = Parser_parseBlockStatement1(p);
    if (func->body == NULL) {
        func->free_fn((DaiAstBase*)func, true);
        return NULL;
    }
    // 末尾分号可选
    if (Parser_peekTokenIs(p, DaiTokenType_semicolon)) {
        Parser_nextToken(p);
    }

    {
        func->end_line   = p->cur_token->end_line;
        func->end_column = p->cur_token->end_column;
    }
    return func;
}

static DaiAstClassVarStatement*
Parser_parseClassVarStatement(Parser* p) {
    DaiAstClassVarStatement* stmt = DaiAstClassVarStatement_New(NULL, NULL);
    stmt->start_line              = p->cur_token->start_line;
    stmt->start_column            = p->cur_token->start_column;

    // 跳过 class ，现在当前 token 是 var
    Parser_nextToken(p);

    // 下一个 token 应该是 identifier
    if (!Parser_expectPeek(p, DaiTokenType_ident)) {
        stmt->free_fn((DaiAstBase*)stmt, true);
        return NULL;
    }

    // 创建 identifier 节点
    DaiAstIdentifier* name = DaiAstIdentifier_New(p->cur_token);
    stmt->name             = name;

    // 下一个 token 应该是 assign
    if (!Parser_expectPeek(p, DaiTokenType_assign)) {
        stmt->free_fn((DaiAstBase*)stmt, true);
        return NULL;
    }
    // 跳过 assign token
    Parser_nextToken(p);

    stmt->value = Parser_parseExpression(p, Precedence_Lowest);
    if (stmt->value == NULL) {
        stmt->free_fn((DaiAstBase*)stmt, true);
        return NULL;
    }

    // 语句结束末尾必须是分号
    if (!Parser_expectPeek(p, DaiTokenType_semicolon)) {
        stmt->free_fn((DaiAstBase*)stmt, true);
        return NULL;
    }

    stmt->end_line   = p->cur_token->end_line;
    stmt->end_column = p->cur_token->end_column;
    return stmt;
}

static DaiAstInsVarStatement*
Parser_parseInsVarStatement(Parser* p) {
    DaiAstInsVarStatement* stmt = DaiAstInsVarStatement_New(NULL, NULL);
    stmt->start_line            = p->cur_token->start_line;
    stmt->start_column          = p->cur_token->start_column;

    // 下一个 token 应该是 identifier
    if (!Parser_expectPeek(p, DaiTokenType_ident)) {
        stmt->free_fn((DaiAstBase*)stmt, true);
        return NULL;
    }

    // 创建 identifier 节点
    DaiAstIdentifier* name = DaiAstIdentifier_New(p->cur_token);
    stmt->name             = name;

    if (Parser_peekTokenIs(p, DaiTokenType_semicolon)) {
        Parser_nextToken(p);
        return stmt;
    }
    // 下一个 token 应该是 assign
    if (!Parser_expectPeek(p, DaiTokenType_assign)) {
        stmt->free_fn((DaiAstBase*)stmt, true);
        return NULL;
    }
    // 跳过 assign token
    Parser_nextToken(p);

    stmt->value = Parser_parseExpression(p, Precedence_Lowest);
    if (stmt->value == NULL) {
        stmt->free_fn((DaiAstBase*)stmt, true);
        return NULL;
    }

    // 语句结束末尾必须是分号
    if (!Parser_expectPeek(p, DaiTokenType_semicolon)) {
        stmt->free_fn((DaiAstBase*)stmt, true);
        return NULL;
    }

    stmt->end_line   = p->cur_token->end_line;
    stmt->end_column = p->cur_token->end_column;
    return stmt;
}

DaiAstMethodStatement*
Parser_parseMethodStatement(Parser* p) {
    DaiAstMethodStatement* func = DaiAstMethodStatement_New();
    {
        func->start_line   = p->cur_token->start_line;
        func->start_column = p->cur_token->start_column;
    }
    if (!Parser_expectPeek(p, DaiTokenType_ident)) {
        func->free_fn((DaiAstBase*)func, true);
        return NULL;
    }
    dai_move(p->cur_token->literal, func->name);

    // 解析函数参数
    if (!Parser_expectPeek(p, DaiTokenType_lparen)) {
        func->free_fn((DaiAstBase*)func, true);
        return NULL;
    }
    func->parameters = Parser_parseFunctionParameters(p, &func->parameters_count);
    if (func->parameters == NULL) {
        func->free_fn((DaiAstBase*)func, true);
        return NULL;
    }
    // 解析函数体
    if (!Parser_expectPeek(p, DaiTokenType_lbrace)) {
        func->free_fn((DaiAstBase*)func, true);
        return NULL;
    }
    func->body = Parser_parseBlockStatement1(p);
    if (func->body == NULL) {
        func->free_fn((DaiAstBase*)func, true);
        return NULL;
    }
    // 末尾分号可选
    if (Parser_peekTokenIs(p, DaiTokenType_semicolon)) {
        Parser_nextToken(p);
    }

    {
        func->end_line   = p->cur_token->end_line;
        func->end_column = p->cur_token->end_column;
    }
    return func;
}

static DaiAstClassStatement*
Parser_parseClassStatement(Parser* p) {
    DaiToken* start_token = p->cur_token;
    // 类名
    if (!Parser_expectPeek(p, DaiTokenType_ident)) {
        return NULL;
    }
    DaiAstClassStatement* klass = DaiAstClassStatement_New(p->cur_token);
    {
        klass->start_line   = start_token->start_line;
        klass->start_column = start_token->start_column;
    }
    if (Parser_peekTokenIs(p, DaiTokenType_lt)) {
        // 父类
        Parser_nextToken(p);
        Parser_nextToken(p);
        klass->parent = (DaiAstIdentifier*)Parser_parseIdentifier(p);
        if (klass->parent == NULL) {
            klass->free_fn((DaiAstBase*)klass, true);
            return NULL;
        }
    }

    if (!Parser_expectPeek(p, DaiTokenType_lbrace)) {
        klass->free_fn((DaiAstBase*)klass, true);
        return NULL;
    }
    DaiAstBlockStatement* body = Parser_parseBlockStatementOfClass(p);
    if (body == NULL) {
        klass->free_fn((DaiAstBase*)klass, true);
        return NULL;
    }
    klass->body = body;

    if (!Parser_expectPeek(p, DaiTokenType_semicolon)) {
        klass->free_fn((DaiAstBase*)klass, true);
        return NULL;
    }
    {
        klass->end_line   = p->cur_token->end_line;
        klass->end_column = p->cur_token->end_column;
    }
    return klass;
}

static DaiAstStatement*
Parser_parseStatementOfClass(Parser* p) {
    switch (p->cur_token->type) {
        case DaiTokenType_class: {
            // class var 语句
            if (Parser_peekTokenIs(p, DaiTokenType_var)) {
                return (DaiAstStatement*)Parser_parseClassVarStatement(p);
            }
            if (Parser_peekTokenIs(p, DaiTokenType_function)) {
                return (DaiAstStatement*)Parser_parseClassMethodStatement(p);
            }
            break;
        }
        case DaiTokenType_var: {
            // var 语句
            return (DaiAstStatement*)Parser_parseInsVarStatement(p);
        }
        case DaiTokenType_function: {
            // method 语句
            return (DaiAstStatement*)Parser_parseMethodStatement(p);
        }
        default: {
            break;
        }
    }
    p->syntax_error = DaiSyntaxError_New(
        "invalid statement in class scope", p->cur_token->start_line, p->cur_token->start_column);
    return NULL;
}

static DaiAstBreakStatement*
Parser_parseBreakStatement(Parser* p) {
    DaiAstBreakStatement* break_stmt = DaiAstBreakStatement_New();
    {
        break_stmt->start_line   = p->cur_token->start_line;
        break_stmt->start_column = p->cur_token->start_column;
    }
    if (!Parser_expectPeek(p, DaiTokenType_semicolon)) {
        break_stmt->free_fn((DaiAstBase*)break_stmt, true);
        return NULL;
    }
    {
        break_stmt->end_line   = p->cur_token->end_line;
        break_stmt->end_column = p->cur_token->end_column;
    }
    return break_stmt;
}

static DaiAstContinueStatement*
Parser_parseContinueStatement(Parser* p) {
    DaiAstContinueStatement* continue_stmt = DaiAstContinueStatement_New();
    {
        continue_stmt->start_line   = p->cur_token->start_line;
        continue_stmt->start_column = p->cur_token->start_column;
    }
    if (!Parser_expectPeek(p, DaiTokenType_semicolon)) {
        continue_stmt->free_fn((DaiAstBase*)continue_stmt, true);
        return NULL;
    }
    {
        continue_stmt->end_line   = p->cur_token->end_line;
        continue_stmt->end_column = p->cur_token->end_column;
    }
    return continue_stmt;
}

DaiAstFunctionStatement*
Parser_parseFunctionStatement(Parser* p) {
    DaiAstFunctionStatement* func = DaiAstFunctionStatement_New();
    {
        func->start_line   = p->cur_token->start_line;
        func->start_column = p->cur_token->start_column;
    }
    if (!Parser_expectPeek(p, DaiTokenType_ident)) {
        func->free_fn((DaiAstBase*)func, true);
        return NULL;
    }
    dai_move(p->cur_token->literal, func->name);

    // 解析函数参数
    if (!Parser_expectPeek(p, DaiTokenType_lparen)) {
        func->free_fn((DaiAstBase*)func, true);
        return NULL;
    }
    func->parameters = Parser_parseFunctionParameters(p, &func->parameters_count);
    if (func->parameters == NULL) {
        func->free_fn((DaiAstBase*)func, true);
        return NULL;
    }
    // 解析函数体
    if (!Parser_expectPeek(p, DaiTokenType_lbrace)) {
        func->free_fn((DaiAstBase*)func, true);
        return NULL;
    }
    func->body = Parser_parseBlockStatement1(p);
    if (func->body == NULL) {
        func->free_fn((DaiAstBase*)func, true);
        return NULL;
    }
    // 末尾分号可选
    if (Parser_peekTokenIs(p, DaiTokenType_semicolon)) {
        Parser_nextToken(p);
    }

    {
        func->end_line   = p->cur_token->end_line;
        func->end_column = p->cur_token->end_column;
    }
    return func;
}

// 解析表达式语句
static DaiAstExpressionStatement*
Parser_parseExpressionStatement(Parser* p) {
    DaiAstExpressionStatement* stmt = DaiAstExpressionStatement_New(NULL);
    stmt->start_line                = p->cur_token->start_line;
    stmt->start_column              = p->cur_token->start_column;
    // 解析表达式
    DaiAstExpression* expr = Parser_parseExpression(p, Precedence_Lowest);
    if (expr == NULL) {
        stmt->free_fn((DaiAstBase*)stmt, false);
        assert(p->syntax_error != NULL);
        return NULL;
    }
    stmt->expression = expr;
    // 下一个 token 应该是语句结尾的分号
    if (!Parser_expectPeek(p, DaiTokenType_semicolon)) {
        stmt->free_fn((DaiAstBase*)stmt, true);
        return NULL;
    }
    stmt->end_line   = p->cur_token->end_line;
    stmt->end_column = p->cur_token->end_column;
    return stmt;
}

// 解析表达式语句或者赋值语句
static DaiAstStatement*
Parser_parseExpressionOrAssignStatement(Parser* p) {
    DaiToken* start_token = p->cur_token;
    // 解析表达式
    DaiAstExpression* expr = Parser_parseExpression(p, Precedence_Lowest);
    if (expr == NULL) {
        assert(p->syntax_error != NULL);
        return NULL;
    }
    DaiAstStatement* dstmt = NULL;
    if (Parser_peekTokenIn(p,
                           DaiTokenType_assign,
                           DaiTokenType_add_assign,
                           DaiTokenType_sub_assign,
                           DaiTokenType_mul_assign,
                           DaiTokenType_div_assign,
                           DaiTokenType_end)) {
        // 赋值语句
        Parser_nextToken(p);
        DaiTokenType assign_type    = p->cur_token->type;
        DaiAstAssignStatement* stmt = DaiAstAssignStatement_New();
        stmt->start_line            = start_token->start_line;
        stmt->start_column          = start_token->start_column;
        stmt->left                  = expr;
        Parser_nextToken(p);
        stmt->value = Parser_parseExpression(p, Precedence_Lowest);
        if (stmt->value == NULL) {
            stmt->free_fn((DaiAstBase*)stmt, true);
            return NULL;
        }
        switch (assign_type) {
            case DaiTokenType_add_assign: stmt->operator= "+"; break;
            case DaiTokenType_sub_assign: stmt->operator= "-"; break;
            case DaiTokenType_mul_assign: stmt->operator= "*"; break;
            case DaiTokenType_div_assign: stmt->operator= "/"; break;
            default: break;
        }
        dstmt = (DaiAstStatement*)stmt;
    } else {
        // 表达式语句
        DaiAstExpressionStatement* stmt = DaiAstExpressionStatement_New(expr);
        stmt->start_line                = start_token->start_line;
        stmt->start_column              = start_token->start_column;
        dstmt                           = (DaiAstStatement*)stmt;
    }
    // 下一个 token 应该是语句结尾的分号
    if (!Parser_expectPeek(p, DaiTokenType_semicolon)) {
        dstmt->free_fn((DaiAstBase*)dstmt, true);
        return NULL;
    }
    dstmt->end_line   = p->cur_token->end_line;
    dstmt->end_column = p->cur_token->end_column;
    return dstmt;
}

static DaiAstReturnStatement*
Parser_parseReturnStatement(Parser* p) {
    DaiAstReturnStatement* stmt = DaiAstReturnStatement_New(NULL);
    stmt->start_line            = p->cur_token->start_line;
    stmt->start_column          = p->cur_token->start_column;

    // 跳过 return 关键字 token
    Parser_nextToken(p);
    if (Parser_curTokenIs(p, DaiTokenType_semicolon)) {
        // "return;" 返回空
        stmt->end_line   = p->cur_token->end_line;
        stmt->end_column = p->cur_token->end_column;
        return stmt;
    }
    // 解析返回值表达式
    stmt->return_value = Parser_parseExpression(p, Precedence_Lowest);
    if (stmt->return_value == NULL) {
        stmt->free_fn((DaiAstBase*)stmt, true);
        return NULL;
    }
    // 语句结尾必须是分号
    if (!Parser_expectPeek(p, DaiTokenType_semicolon)) {
        stmt->free_fn((DaiAstBase*)stmt, true);
        return NULL;
    }

    stmt->end_line   = p->cur_token->end_line;
    stmt->end_column = p->cur_token->end_column;
    return stmt;
}


static DaiAstVarStatement*
Parser_parseVarStatement(Parser* p) {
    DaiAstVarStatement* stmt = DaiAstVarStatement_New(NULL, NULL);
    stmt->start_line         = p->cur_token->start_line;
    stmt->start_column       = p->cur_token->start_column;

    // 下一个 token 应该是 identifier
    if (!Parser_expectPeek(p, DaiTokenType_ident)) {
        stmt->free_fn((DaiAstBase*)stmt, true);
        return NULL;
    }

    // 创建 identifier 节点
    DaiAstIdentifier* name = DaiAstIdentifier_New(p->cur_token);
    stmt->name             = name;

    // 下一个 token 应该是 assign
    if (!Parser_expectPeek(p, DaiTokenType_assign)) {
        stmt->free_fn((DaiAstBase*)stmt, true);
        return NULL;
    }
    // 跳过 assign token
    Parser_nextToken(p);

    stmt->value = Parser_parseExpression(p, Precedence_Lowest);
    if (stmt->value == NULL) {
        stmt->free_fn((DaiAstBase*)stmt, true);
        return NULL;
    }

    // 语句结束末尾必须是分号
    if (!Parser_expectPeek(p, DaiTokenType_semicolon)) {
        stmt->free_fn((DaiAstBase*)stmt, true);
        return NULL;
    }

    stmt->end_line   = p->cur_token->end_line;
    stmt->end_column = p->cur_token->end_column;
    return stmt;
}

static DaiAstIfStatement*
Parser_parseIfStatement(Parser* p) {
    DaiAstIfStatement* ifstatement = DaiAstIfStatement_New();
    {
        ifstatement->start_line   = p->cur_token->start_line;
        ifstatement->start_column = p->cur_token->start_column;
    }

    // 解析条件
    if (!Parser_expectPeek(p, DaiTokenType_lparen)) {
        ifstatement->free_fn((DaiAstBase*)ifstatement, true);
        return NULL;
    }
    Parser_nextToken(p);
    ifstatement->condition = (DaiAstExpression*)Parser_parseExpression(p, Precedence_Lowest);
    if (ifstatement->condition == NULL) {
        ifstatement->free_fn((DaiAstBase*)ifstatement, true);
        return NULL;
    }
    if (!Parser_expectPeek(p, DaiTokenType_rparen)) {
        ifstatement->free_fn((DaiAstBase*)ifstatement, true);
        return NULL;
    }
    // 解析 if 语句块
    if (!Parser_expectPeek(p, DaiTokenType_lbrace)) {
        ifstatement->free_fn((DaiAstBase*)ifstatement, true);
        return NULL;
    }
    ifstatement->then_branch = Parser_parseBlockStatement1(p);
    if (ifstatement->then_branch == NULL) {
        ifstatement->free_fn((DaiAstBase*)ifstatement, true);
        return NULL;
    }
    // 解析 elif 语句块
    while (Parser_peekTokenIs(p, DaiTokenType_elif)) {
        Parser_nextToken(p);
        if (!Parser_expectPeek(p, DaiTokenType_lparen)) {
            ifstatement->free_fn((DaiAstBase*)ifstatement, true);
            return NULL;
        }
        Parser_nextToken(p);
        DaiAstExpression* condition =
            (DaiAstExpression*)Parser_parseExpression(p, Precedence_Lowest);
        if (condition == NULL) {
            ifstatement->free_fn((DaiAstBase*)ifstatement, true);
            return NULL;
        }
        if (!Parser_expectPeek(p, DaiTokenType_rparen)) {
            condition->free_fn((DaiAstBase*)condition, true);
            ifstatement->free_fn((DaiAstBase*)ifstatement, true);
            return NULL;
        }
        if (!Parser_expectPeek(p, DaiTokenType_lbrace)) {
            condition->free_fn((DaiAstBase*)condition, true);
            ifstatement->free_fn((DaiAstBase*)ifstatement, true);
            return NULL;
        }
        DaiAstBlockStatement* elif_branch = Parser_parseBlockStatement1(p);
        if (elif_branch == NULL) {
            condition->free_fn((DaiAstBase*)condition, true);
            ifstatement->free_fn((DaiAstBase*)ifstatement, true);
            return NULL;
        }
        DaiAstIfStatement_append_elif_branch(ifstatement, condition, elif_branch);
    }
    // 解析 else 语句块
    if (Parser_peekTokenIs(p, DaiTokenType_else)) {
        Parser_nextToken(p);
        if (!Parser_expectPeek(p, DaiTokenType_lbrace)) {
            ifstatement->free_fn((DaiAstBase*)ifstatement, true);
            return NULL;
        }
        ifstatement->else_branch = Parser_parseBlockStatement1(p);
        if (ifstatement->else_branch == NULL) {
            ifstatement->free_fn((DaiAstBase*)ifstatement, true);
            return NULL;
        }
    }
    // 末尾分号可选
    if (Parser_peekTokenIs(p, DaiTokenType_semicolon)) {
        Parser_nextToken(p);
    }
    {
        ifstatement->end_line   = p->cur_token->end_line;
        ifstatement->end_column = p->cur_token->end_column;
    }
    return ifstatement;
}

static DaiAstWhileStatement*
Parser_parseWhileStatement(Parser* p) {
    DaiAstWhileStatement* while_stmt = DaiAstWhileStatement_New();
    {
        while_stmt->start_line   = p->cur_token->start_line;
        while_stmt->start_column = p->cur_token->start_column;
    }
    // 解析条件
    if (!Parser_expectPeek(p, DaiTokenType_lparen)) {
        while_stmt->free_fn((DaiAstBase*)while_stmt, true);
        return NULL;
    }
    Parser_nextToken(p);
    while_stmt->condition = (DaiAstExpression*)Parser_parseExpression(p, Precedence_Lowest);
    if (while_stmt->condition == NULL) {
        while_stmt->free_fn((DaiAstBase*)while_stmt, true);
        return NULL;
    }
    if (!Parser_expectPeek(p, DaiTokenType_rparen)) {
        while_stmt->free_fn((DaiAstBase*)while_stmt, true);
        return NULL;
    }
    // 解析 while 语句块
    if (!Parser_expectPeek(p, DaiTokenType_lbrace)) {
        while_stmt->free_fn((DaiAstBase*)while_stmt, true);
        return NULL;
    }
    while_stmt->body = Parser_parseBlockStatement1(p);
    if (while_stmt->body == NULL) {
        while_stmt->free_fn((DaiAstBase*)while_stmt, true);
        return NULL;
    }
    if (Parser_peekTokenIs(p, DaiTokenType_semicolon)) {
        Parser_nextToken(p);
    }
    {
        while_stmt->end_line   = p->cur_token->end_line;
        while_stmt->end_column = p->cur_token->end_column;
    }
    return while_stmt;
}

static DaiAstBlockStatement*
Parser_parseBlockStatement1(Parser* p) {
    DaiAstBlockStatement* blockstatement = DaiAstBlockStatement_New();
    blockstatement->start_line           = p->cur_token->start_line;
    blockstatement->start_column         = p->cur_token->start_column;
    Parser_nextToken(p);

    while (!Parser_curTokenIs(p, DaiTokenType_rbrace) && !Parser_curTokenIs(p, DaiTokenType_eof)) {
        DaiAstStatement* stmt = Parser_parseStatement(p);
        if (stmt == NULL) {
            blockstatement->free_fn((DaiAstBase*)blockstatement, true);
            return NULL;
        }
        DaiAstBlockStatement_append(blockstatement, stmt);
        Parser_nextToken(p);
    }
    if (!Parser_curTokenIs(p, DaiTokenType_rbrace)) {
        int line        = p->cur_token->start_line;
        int column      = p->cur_token->start_column;
        p->syntax_error = DaiSyntaxError_New("expected '}'", line, column);
        blockstatement->free_fn((DaiAstBase*)blockstatement, true);
        return NULL;
    }
    blockstatement->end_line   = p->cur_token->end_line;
    blockstatement->end_column = p->cur_token->end_column;
    return blockstatement;
}

static DaiAstBlockStatement*
Parser_parseBlockStatement(Parser* p) {
    DaiAstBlockStatement* blockstatement = Parser_parseBlockStatement1(p);
    if (blockstatement == NULL) {
        return NULL;
    }
    // 末尾分号可选
    if (Parser_peekTokenIs(p, DaiTokenType_semicolon)) {
        Parser_nextToken(p);
    }
    blockstatement->end_line   = p->cur_token->end_line;
    blockstatement->end_column = p->cur_token->end_column;
    return blockstatement;
}

static DaiAstStatement*
Parser_parseStatement(Parser* p) {
    switch (p->cur_token->type) {
        case DaiTokenType_class: {
            // class 语句
            return (DaiAstStatement*)Parser_parseClassStatement(p);
        }
        case DaiTokenType_var: {
            // var 语句
            return (DaiAstStatement*)Parser_parseVarStatement(p);
        }
        case DaiTokenType_return: {
            // return 语句
            return (DaiAstStatement*)Parser_parseReturnStatement(p);
        }
        case DaiTokenType_if: {
            // if 语句
            return (DaiAstStatement*)Parser_parseIfStatement(p);
        }
        case DaiTokenType_function: {
            if (Parser_peekTokenIs(p, DaiTokenType_lparen)) {
                // 表达式语句
                return (DaiAstStatement*)Parser_parseExpressionStatement(p);
            } else {
                // function 语句
                return (DaiAstStatement*)Parser_parseFunctionStatement(p);
            }
        }
        case DaiTokenType_break: {
            // break 语句
            return (DaiAstStatement*)Parser_parseBreakStatement(p);
        }
        case DaiTokenType_continue: {
            // continue 语句
            return (DaiAstStatement*)Parser_parseContinueStatement(p);
        }
        case DaiTokenType_while: {
            // while 语句
            return (DaiAstStatement*)Parser_parseWhileStatement(p);
        }
        case DaiTokenType_self:
        case DaiTokenType_super:
        case DaiTokenType_ident: {
            return Parser_parseExpressionOrAssignStatement(p);
        }
        case DaiTokenType_lbrace: {
            return (DaiAstStatement*)Parser_parseBlockStatement(p);
        }
        default: {
            return (DaiAstStatement*)Parser_parseExpressionStatement(p);
        }
    }
}

static DaiSyntaxError*
Parser_parseProgram(Parser* p, DaiAstProgram* program) {

    while (p->cur_token->type != DaiTokenType_eof) {
        DaiAstStatement* stmt = Parser_parseStatement(p);
        if (stmt == NULL) {
            // 语法错误，直接返回
            break;
        }
        DaiAstProgram_append(program, stmt);
        // 一个语句解析完成后， cur_token 是语句的末尾的 token
        // 所以需要读取下一个 token
        Parser_nextToken(p);
    }
    return p->syntax_error;
}
// #endregion

DaiSyntaxError*
dai_parse(DaiTokenList* tlist, DaiAstProgram* program) {
    // 创建解析器
    Parser* parser = Parser_New(tlist);
    // 解析 token 列表，构建 ast
    DaiSyntaxError* err = Parser_parseProgram(parser, program);
    // 释放解析器
    Parser_free(parser);
    return err;
}