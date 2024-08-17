#ifndef A25C9E69_F6E2_48F1_B1E6_8E313B3144F7
#define A25C9E69_F6E2_48F1_B1E6_8E313B3144F7

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


#endif /* A25C9E69_F6E2_48F1_B1E6_8E313B3144F7 */
