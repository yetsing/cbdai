#ifndef CD2D8A9E_2AE5_4886_B1AB_69B44C7EC388
#define CD2D8A9E_2AE5_4886_B1AB_69B44C7EC388

DaiAstExpression*
Parser_parseGroupedExpression(Parser* p) {
    Parser_nextToken(p);
    DaiAstExpression* exp = Parser_parseExpression(p, Precedence_Lowest);
    if (exp == NULL) {
        return NULL;
    }
    if (!Parser_expectPeek(p, DaiTokenType_rparen)) {
        exp->free_fn((DaiAstBase*)exp, true);
        return NULL;
    }
    return exp;
}

#endif /* CD2D8A9E_2AE5_4886_B1AB_69B44C7EC388 */
