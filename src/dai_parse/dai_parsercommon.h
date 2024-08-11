//
// Created by  on 2024/6/5.
//

#ifndef CBDAI_SRC_DAI_PARSER_DAI_PARSERCOMMON_H_
#define CBDAI_SRC_DAI_PARSER_DAI_PARSERCOMMON_H_

#include "dai_ast.h"
#include "dai_tokenize.h"

typedef struct _Parser Parser;

// 解析函数需要遵循一个约定
// 函数在开始解析表达式时， cur_token 必须是当前所关联的 token 类型，
// 返回分析的表达式结果时， cur_token 是当前表达式类型中的最后一个 token
// 比如 5 + 4 解析
// 开始解析时， cur_token = 5
// 返回分析的表达式结果时， cur_token = 4
// 前缀表达式解析函数
typedef DaiAstExpression *(*prefixParseFn)(Parser *p);
// 中缀表达式解析函数
typedef DaiAstExpression *(*infixParseFn)(Parser *p, DaiAstExpression *left);

// 表达式优先级
typedef enum {
  Precedence_none = 0,
  // 优先级最低
  Precedence_Lowest,
  Precedence_Equals, // ==
  Precedence_LessGreater, // > or <
  Precedence_Sum, // +
  Precedence_Product, // *
  Precedence_Prefix,  // -X or !X
  Precedence_Call, // myFunction(X)
  // 优先级最高
  Precedence_Highest,

} Precedence;

static Precedence token_precedences(DaiTokenType type) {
    switch (type) {
    case DaiTokenType_eq:
      return Precedence_Equals;
    case DaiTokenType_not_eq:
      return Precedence_Equals;
    case DaiTokenType_lt:
      return Precedence_LessGreater;
    case DaiTokenType_gt:
      return Precedence_LessGreater;
    case DaiTokenType_minus:
      return Precedence_Sum;
    case DaiTokenType_plus:
      return Precedence_Sum;
    case DaiTokenType_slash:
      return Precedence_Product;
    case DaiTokenType_asterisk:
      return Precedence_Product;
    case DaiTokenType_lparen:
    case DaiTokenType_dot:
      return Precedence_Call;
    default:
      return Precedence_Lowest;
    }
}

typedef struct _Parser {
  DaiTokenList *tlist;

  // 上一个 token ，帮助展示更准确的错误信息
  DaiToken *prev_token;
  // 当前 token
  DaiToken *cur_token;
  // 下一个 token
  DaiToken *peek_token;

  // DaiTokenType 对应前缀、中缀解析函数
  prefixParseFn prefix_parse_fns[DaiTokenType_end];
  infixParseFn infix_parse_fns[DaiTokenType_end];

  // syntax error 需要返回给调用方，所以不应该由 parser 释放其内存
  DaiSyntaxError *syntax_error;
} Parser;

//#region parser 辅助方法声明

static Parser *Parser_New(DaiTokenList *tlist);
static void Parser_free(Parser *p);
static void Parser_nextToken(Parser *p);
static bool Parser_curTokenIs(Parser *p, DaiTokenType type);
static bool Parser_peekTokenIs(Parser *p, DaiTokenType type);
static bool Parser_expectPeek(Parser *p, DaiTokenType type);

static Precedence Parser_curPrecedence(Parser *p);
static Precedence Parser_peekPrecedence(Parser *p);

static void
Parser_register(Parser *p);
static void
Parser_registerPrefix(Parser *p, DaiTokenType type, prefixParseFn fn);
static void
Parser_registerInfix(Parser *p, DaiTokenType type, infixParseFn fn);
//#endregion

//#region parser 表达式解析方法声明

static DaiAstExpression *Parser_parseInteger(Parser *p);
static DaiAstExpression *Parser_parseIdentifier(Parser *p);
static DaiAstExpression *Parser_parseBoolean(Parser *p);
static DaiAstExpression *Parser_parsePrefixExpression(Parser *p);
static DaiAstExpression *Parser_parseInfixExpression(Parser *p, DaiAstExpression *left);
static DaiAstExpression *Parser_parseGroupedExpression(Parser *p);
static DaiAstExpression *Parser_parseCallExpression(Parser *p, DaiAstExpression *left);
static DaiAstExpression *Parser_parseDotExpression(Parser *p, DaiAstExpression *left);
static DaiAstExpression *Parser_parseSelfExpression(Parser *p);
static DaiAstExpression *Parser_parseSuperExpression(Parser *p);
static DaiAstExpression *Parser_parseFunctionLiteral(Parser *p);
static DaiAstExpression *Parser_parseStringLiteral(Parser *p);
static DaiAstExpression *Parser_parseExpression(Parser *p, Precedence precedence);

static DaiAstStatement *Parser_parseStatement(Parser *p);
static DaiAstBlockStatement *Parser_parseBlockStatement(Parser *p);
static DaiAstClassStatement *Parser_parseClassStatement(Parser *p);
static DaiAstBlockStatement *Parser_parseBlockStatementOfClass(Parser *p);
static DaiAstStatement *Parser_parseStatementOfClass(Parser *p);
//#endregion

#endif
