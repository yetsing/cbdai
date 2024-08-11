#ifndef F1C63E75_2447_49CB_BF70_826ACFD540E8
#define F1C63E75_2447_49CB_BF70_826ACFD540E8

static DaiAstExpression *Parser_parseDotExpression(Parser *p, DaiAstExpression *left) {
    DaiAstDotExpression *expr = DaiAstDotExpression_New(left);
    {
        expr->start_line = left->start_line;
        expr->start_column = left->start_column;
    }
    if (!Parser_expectPeek(p, DaiTokenType_ident)) {
        expr->free_fn((DaiAstBase *)expr, true);
        return NULL;
    }
    DaiAstIdentifier *name =(DaiAstIdentifier *) Parser_parseIdentifier(p);
    expr->name = name;
    {
        expr->end_line = name->end_line;
        expr->end_column = name->end_column;
    }
    return (DaiAstExpression *)expr;
}

#endif /* F1C63E75_2447_49CB_BF70_826ACFD540E8 */
