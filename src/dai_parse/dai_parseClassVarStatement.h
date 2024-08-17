#ifndef E1F3E8E5_7706_41A6_B9F3_FF065A4AD63B
#define E1F3E8E5_7706_41A6_B9F3_FF065A4AD63B


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


#endif /* E1F3E8E5_7706_41A6_B9F3_FF065A4AD63B */
