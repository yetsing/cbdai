//
// Created by on 2024/6/5.
//

#ifndef CBAI_SRC_DAI_PARSER_DAI_PARSERBASE_H_
#define CBAI_SRC_DAI_PARSER_DAI_PARSERBASE_H_
#include <stdio.h>
#include <string.h>

#include "dai_assert.h"
#include "dai_malloc.h"
#include "dai_tokenize.h"

// include c file
#include "dai_parse/dai_parsercommon.h"
#include "dai_parse/dai_parserregister.h"

static Parser *Parser_New(DaiTokenList *tlist) {
  Parser *p = dai_malloc(sizeof(Parser));
  p->tlist = tlist;
  p->prev_token = NULL;
  p->cur_token = NULL;
  p->peek_token = NULL;
  p->syntax_error = NULL;

  // 读取两个 token ，以设置 cur_token 和 peek_token
  Parser_nextToken(p);
  Parser_nextToken(p);
  assert(p->cur_token && p->peek_token);

  Parser_register(p);
  return p;
}

// 读取下一个 token
static void Parser_nextToken(Parser *p) {
  p->prev_token = p->cur_token;
  p->cur_token = p->peek_token;
  p->peek_token = DaiTokenList_next(p->tlist);
  // 跳过注释
  while (p->cur_token && p->cur_token->type == DaiTokenType_comment) {
    p->prev_token = p->cur_token;
    p->cur_token = p->peek_token;
    p->peek_token = DaiTokenList_next(p->tlist);
  }
  while (p->peek_token && p->peek_token->type == DaiTokenType_comment) {
    p->peek_token = DaiTokenList_next(p->tlist);
  }
}

// 判断当前 token 是否为期望的 token 类型
static bool Parser_curTokenIs(Parser *p, DaiTokenType type) {
  return p->cur_token->type == type;
}

// 判断下一个 token 是否为期望的 token 类型
static bool Parser_peekTokenIs(Parser *p, DaiTokenType type) {
  return p->peek_token->type == type;
}

// 检查下一个 token 是否为期望的 token 类型
// 如果是，则读取下一个 token
// 否则设置 syntax_error 并返回 false
static bool Parser_expectPeek(Parser *p, DaiTokenType type) {
  if (Parser_peekTokenIs(p, type)) {
    Parser_nextToken(p);
    return true;
  }
  char buf[128];
  snprintf(buf, 128, "expected token to be \"%s\" but got \"%s\"",
           DaiTokenType_string(type), DaiTokenType_string(p->peek_token->type));
  int line = p->peek_token->start_line;
  int column = p->peek_token->start_column;
  if (p->peek_token->type == DaiTokenType_eof) {
    line = p->cur_token->end_line;
    column = p->cur_token->end_column;
  }
  p->syntax_error = DaiSyntaxError_New(buf, line, column);
  return false;
}

// 获取当前 token 的优先级
static Precedence Parser_curPrecedence(Parser *p) {
  return token_precedences(p->cur_token->type);
}

// 获取下一个 token 的优先级
static Precedence Parser_peekPrecedence(Parser *p) {
  return token_precedences(p->peek_token->type);
}

// 注册前缀表达式解析函数
static void Parser_registerPrefix(Parser *p, DaiTokenType type,
                                  prefixParseFn fn) {
  p->prefix_parse_fns[type] = fn;
}

// 注册中缀表达式解析函数
static void Parser_registerInfix(Parser *p, DaiTokenType type,
                                 infixParseFn fn) {
  p->infix_parse_fns[type] = fn;
}

// 释放解析器
static void Parser_free(Parser *p) {
  dai_free(p);
}

#endif