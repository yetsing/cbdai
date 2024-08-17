
#ifndef CBDAI_SRC_DAI_PARSER_DAI_PARSEEXPRESSIONORASSIGNSTATEMENT_H
#define CBDAI_SRC_DAI_PARSER_DAI_PARSEEXPRESSIONORASSIGNSTATEMENT_H

#include "dai_parse/dai_parseexpression.h"

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
    if (Parser_peekTokenIs(p, DaiTokenType_assign)) {
        // 赋值语句
        if (!Parser_expectPeek(p, DaiTokenType_assign)) {
            expr->free_fn((DaiAstBase*)expr, true);
            return NULL;
        }
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

#endif   // CBDAI_SRC_DAI_PARSER_DAI_PARSEEXPRESSIONORASSIGNSTATEMENT_H
