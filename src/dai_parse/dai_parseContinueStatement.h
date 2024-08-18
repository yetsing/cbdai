#ifndef E4EBA6BB_2199_432E_8141_D4295EEC9266
#define E4EBA6BB_2199_432E_8141_D4295EEC9266

static DaiAstContinueStatement*
Parser_parseContinueStatement(Parser* p) {
    DaiAstContinueStatement* continue_stmt = DaiAstContinueStatement_New();
    {
        continue_stmt->start_line   = p->cur_token->start_line;
        continue_stmt->start_column = p->cur_token->start_column;
    }
    if (!Parser_expectPeek(p, DaiTokenType_semicolon)) {
        continue_stmt->free_fn((DaiAstBase*)continue_stmt, true);
        return NULL;
    }
    {
        continue_stmt->end_line   = p->cur_token->end_line;
        continue_stmt->end_column = p->cur_token->end_column;
    }
    return continue_stmt;
}

#endif /* E4EBA6BB_2199_432E_8141_D4295EEC9266 */
