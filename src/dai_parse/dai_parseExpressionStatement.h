//
// Created by  on 2024/6/5.
//

#ifndef CBDAI_SRC_DAI_PARSER_DAI_PARSEEXPRESSIONSTATEMENT_H_
#define CBDAI_SRC_DAI_PARSER_DAI_PARSEEXPRESSIONSTATEMENT_H_

#include "dai_parse/dai_parseExpression.h"

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

#endif   // CBDAI_SRC_DAI_PARSER_DAI_PARSEEXPRESSIONSTATEMENT_H_
