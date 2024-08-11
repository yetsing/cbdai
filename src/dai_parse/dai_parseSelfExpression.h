#ifndef FC244966_229E_460F_874E_28724CDEEA22
#define FC244966_229E_460F_874E_28724CDEEA22

static DaiAstExpression *Parser_parseSelfExpression(Parser *p) {
    DaiAstSelfExpression *expr = DaiAstSelfExpression_New();
    {
        expr->start_line = p->cur_token->start_line;
        expr->start_column = p->cur_token->start_column;
    }
    if (!Parser_peekTokenIs(p, DaiTokenType_dot)) {
        expr->end_line = p->cur_token->end_line;
        expr->end_column = p->cur_token->end_column;
        return (DaiAstExpression *) expr;
    }
    if (!Parser_expectPeek(p, DaiTokenType_dot)) {
        expr->free_fn((DaiAstBase *)expr, true);
        return NULL;
    }
    if (!Parser_expectPeek(p, DaiTokenType_ident)) {
        expr->free_fn((DaiAstBase *)expr, true);
        return NULL;
    }
    DaiAstIdentifier *name = (DaiAstIdentifier *) Parser_parseIdentifier(p);
    expr->name = name;
    {
        expr->end_line = name->end_line;
        expr->end_column = name->end_column;
    }
    return (DaiAstExpression *)expr;
}

#endif /* FC244966_229E_460F_874E_28724CDEEA22 */
