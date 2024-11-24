#include <assert.h>
#include <stdio.h>

#include "dai_parse.h"
#include "dai_tokenize.h"

#include "dai_assert.h"
#include "dai_malloc.h"

#include "dai_ast.h"

typedef struct _Parser Parser;   // NOLINT(*-reserved-identifier)

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
    Precedence_Or,        // or
    Precedence_And,       // and
    Precedence_Not,       // not
    Precedence_Compare,   // > or <
    Precedence_Sum,       // +
    Precedence_Product,   // * / %
    Precedence_Prefix,    // -X or !X
    Precedence_Call,      // myFunction(X)
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
        case DaiTokenType_minus:
        case DaiTokenType_plus: return Precedence_Sum;
        case DaiTokenType_percent:
        case DaiTokenType_slash:
        case DaiTokenType_asterisk: return Precedence_Product;
        case DaiTokenType_lparen:
        case DaiTokenType_dot: return Precedence_Call;
        default: return Precedence_Lowest;
    }
}

typedef struct _Parser {
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

// #region parser 表达式解析方法声明

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
Parser_parseExpression(Parser* p, Precedence precedence);

static DaiAstStatement*
Parser_parseStatement(Parser* p);
static DaiAstBlockStatement*
Parser_parseBlockStatement(Parser* p);
// #endregion

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
    int line   = p->peek_token->start_line;
    int column = p->peek_token->start_column;
    if (p->peek_token->type == DaiTokenType_eof) {
        line   = p->cur_token->end_line;
        column = p->cur_token->end_column;
    }
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
        Parser_registerPrefix(p, DaiTokenType_true, Parser_parseBoolean);
        Parser_registerPrefix(p, DaiTokenType_false, Parser_parseBoolean);
        Parser_registerPrefix(p, DaiTokenType_nil, Parser_parseNil);
        Parser_registerPrefix(p, DaiTokenType_lparen, Parser_parseGroupedExpression);
        Parser_registerPrefix(p, DaiTokenType_function, Parser_parseFunctionLiteral);
        Parser_registerPrefix(p, DaiTokenType_str, Parser_parseStringLiteral);
        Parser_registerPrefix(p, DaiTokenType_self, Parser_parseSelfExpression);
        Parser_registerPrefix(p, DaiTokenType_super, Parser_parseSuperExpression);
    }

    // 注册中缀表达式解析函数
    {
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
    }
}

// #region 解析方法

#include "dai_parse/dai_parseBreakStatement.h"
#include "dai_parse/dai_parseClassStatement.h"
#include "dai_parse/dai_parseContinueStatement.h"
#include "dai_parse/dai_parseExpressionOrAssignStatement.h"
#include "dai_parse/dai_parseExpressionStatement.h"
#include "dai_parse/dai_parseFunctionStatement.h"
#include "dai_parse/dai_parseIfStatement.h"
#include "dai_parse/dai_parseReturnStatement.h"
#include "dai_parse/dai_parseVarStatement.h"
#include "dai_parse/dai_parseWhileStatement.h"

static DaiAstBlockStatement*
Parser_parseBlockStatement(Parser* p) {
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
        int line   = p->cur_token->start_line;
        int column = p->cur_token->start_column;
        if (p->cur_token->type == DaiTokenType_eof) {
            line   = p->prev_token->end_line;
            column = p->prev_token->end_column;
        }
        p->syntax_error = DaiSyntaxError_New("expected '}'", line, column);
        blockstatement->free_fn((DaiAstBase*)blockstatement, true);
        return NULL;
    }
    blockstatement->start_line   = p->cur_token->start_line;
    blockstatement->start_column = p->cur_token->start_column;
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