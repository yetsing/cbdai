#ifndef AE266035_DE3C_42D5_8F1E_23ED3A52DD80
#define AE266035_DE3C_42D5_8F1E_23ED3A52DD80

static DaiAstExpression *Parser_parseSuperExpression(Parser *p) {
    DaiAstSuperExpression *expr = DaiAstSuperExpression_New();
    {
        expr->start_line = p->cur_token->start_line;
        expr->start_column = p->cur_token->start_column;
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

#endif /* AE266035_DE3C_42D5_8F1E_23ED3A52DD80 */
