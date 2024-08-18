#ifndef FB0F34F7_60BE_415E_973B_5CC0D8296CAD
#define FB0F34F7_60BE_415E_973B_5CC0D8296CAD

static DaiAstBreakStatement*
Parser_parseBreakStatement(Parser* p) {
    DaiAstBreakStatement* break_stmt = DaiAstBreakStatement_New();
    {
        break_stmt->start_line   = p->cur_token->start_line;
        break_stmt->start_column = p->cur_token->start_column;
    }
    if (!Parser_expectPeek(p, DaiTokenType_semicolon)) {
        break_stmt->free_fn((DaiAstBase*)break_stmt, true);
        return NULL;
    }
    {
        break_stmt->end_line   = p->cur_token->end_line;
        break_stmt->end_column = p->cur_token->end_column;
    }
    return break_stmt;
}

#endif /* FB0F34F7_60BE_415E_973B_5CC0D8296CAD */
