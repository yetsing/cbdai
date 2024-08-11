#ifndef CF959D87_79F8_483E_88A8_FB1A7C050F6D
#define CF959D87_79F8_483E_88A8_FB1A7C050F6D

static DaiAstIfStatement *Parser_parseIfStatement(Parser *p) {
    DaiAstIfStatement *ifstatement = DaiAstIfStatement_New();
    {
        ifstatement->start_line = p->cur_token->start_line;
        ifstatement->start_column = p->cur_token->start_column;
    }

    // 解析条件
    if (!Parser_expectPeek(p, DaiTokenType_lparen)) {
        ifstatement->free_fn((DaiAstBase *)ifstatement, true);
        return NULL;
    }
    Parser_nextToken(p);
    ifstatement->condition = (DaiAstExpression *)Parser_parseExpression(p, Precedence_Lowest);
    if (ifstatement->condition == NULL) {
        ifstatement->free_fn((DaiAstBase *)ifstatement, true);
        return NULL;
    }
    if (!Parser_expectPeek(p, DaiTokenType_rparen)) {
        ifstatement->free_fn((DaiAstBase *)ifstatement, true);
        return NULL;
    }
    // 解析 if 语句块
    if (!Parser_expectPeek(p, DaiTokenType_lbrace)) {
        ifstatement->free_fn((DaiAstBase *)ifstatement, true);
        return NULL;
    }
    ifstatement->then_branch = Parser_parseBlockStatement(p);
    if (ifstatement->then_branch == NULL) {
        ifstatement->free_fn((DaiAstBase *)ifstatement, true);
        return NULL;
    }
    // 解析 else 语句块
    if (Parser_peekTokenIs(p, DaiTokenType_else)) {
        Parser_nextToken(p);
        if (!Parser_expectPeek(p, DaiTokenType_lbrace)) {
            ifstatement->free_fn((DaiAstBase *)ifstatement, true);
            return NULL;
        }
        ifstatement->else_branch = Parser_parseBlockStatement(p);
        if (ifstatement->else_branch == NULL) {
            ifstatement->free_fn((DaiAstBase *)ifstatement, true);
            return NULL;
        }
    }
    if (!Parser_expectPeek(p, DaiTokenType_semicolon)) {
        ifstatement->free_fn((DaiAstBase *)ifstatement, true);
        return NULL;
    }
    {
        ifstatement->end_line = p->cur_token->end_line;
        ifstatement->end_column = p->cur_token->end_column;
    }
    return ifstatement;
}

#endif /* CF959D87_79F8_483E_88A8_FB1A7C050F6D */
