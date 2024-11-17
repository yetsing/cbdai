//
// Created by yeqing on 24-11-17.
//

// ReSharper disable CppUnusedIncludeDirective
#ifndef CBDAI_SRC_DAI_PARSER_DAI_PARSEEXPRESSION_H_
#define CBDAI_SRC_DAI_PARSER_DAI_PARSEEXPRESSION_H_

#include "dai_parse/dai_parseDotExpression.h"
#include "dai_parse/dai_parseNil.h"
#include "dai_parse/dai_parseSelfExpression.h"
#include "dai_parse/dai_parseSuperExpression.h"
#include "dai_parse/dai_parseBoolean.h"
#include "dai_parse/dai_parseCallExpression.h"
#include "dai_parse/dai_parseFunctionLiteral.h"
#include "dai_parse/dai_parseGroupedExpression.h"
#include "dai_parse/dai_parseIdentifier.h"
#include "dai_parse/dai_parseInfixExpression.h"
#include "dai_parse/dai_parseInteger.h"
#include "dai_parse/dai_parsePrefixExpression.h"
#include "dai_parse/dai_parseStringLiteral.h"


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
            int line   = p->cur_token->start_line;
            int column = p->cur_token->start_column;
            if (p->cur_token->type == DaiTokenType_eof) {
                line   = p->prev_token->end_line;
                column = p->prev_token->end_column;
            }
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

#endif //CBDAI_SRC_DAI_PARSER_DAI_PARSEEXPRESSION_H_
