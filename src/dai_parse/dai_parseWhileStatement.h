#ifndef DDDF5790_DA7A_4366_82A3_A208697A5552
#define DDDF5790_DA7A_4366_82A3_A208697A5552

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
    while_stmt->body = Parser_parseBlockStatement(p);
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

#endif /* DDDF5790_DA7A_4366_82A3_A208697A5552 */
