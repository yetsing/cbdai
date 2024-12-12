#ifndef AB48B11F_9945_4863_A27A_FE167B24E2FF
#define AB48B11F_9945_4863_A27A_FE167B24E2FF


static DaiAstExpression*
Parser_parseCallExpression(Parser* p, DaiAstExpression* left) {
    DaiAstCallExpression* call = DaiAstCallExpression_New();
    {
        call->start_line   = left->start_line;
        call->start_column = left->start_column;
    }
    call->function  = left;
    call->arguments = Parser_parseExpressionList(p, DaiTokenType_rparen, &call->arguments_count);
    if (call->arguments == NULL) {
        call->free_fn((DaiAstBase*)call, true);
        return NULL;
    }
    {
        call->end_line   = p->cur_token->end_line;
        call->end_column = p->cur_token->end_column;
    }
    return (DaiAstExpression*)call;
}

#endif /* AB48B11F_9945_4863_A27A_FE167B24E2FF */
