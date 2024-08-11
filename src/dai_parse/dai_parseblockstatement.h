#ifndef A48E84EA_76E2_482D_A8B9_10441BB5D12D
#define A48E84EA_76E2_482D_A8B9_10441BB5D12D

static DaiAstBlockStatement *Parser_parseBlockStatement(Parser *p) {
    DaiAstBlockStatement *blockstatement = DaiAstBlockStatement_New();
    blockstatement->start_line = p->cur_token->start_line;
    blockstatement->start_column = p->cur_token->start_column;
    Parser_nextToken(p);

    while (!Parser_curTokenIs(p, DaiTokenType_rbrace) && !Parser_curTokenIs(p, DaiTokenType_eof)) {
        DaiAstStatement *stmt = Parser_parseStatement(p);
        if (stmt == NULL) {
            blockstatement->free_fn((DaiAstBase *)blockstatement, true);
            return NULL;
        }
        DaiAstBlockStatement_append(blockstatement, stmt);
        Parser_nextToken(p);
    }
    if (!Parser_curTokenIs(p, DaiTokenType_rbrace)) {
        int line = p->cur_token->start_line;
        int column = p->cur_token->start_column;
        if (p->cur_token->type == DaiTokenType_eof) {
            line = p->prev_token->end_line;
            column = p->prev_token->end_column;
        }
        p->syntax_error = DaiSyntaxError_New("expected '}'", line, column);
        blockstatement->free_fn((DaiAstBase *)blockstatement, true);
        return NULL;
    }
    blockstatement->start_line = p->cur_token->start_line;
    blockstatement->start_column = p->cur_token->start_column;
    return blockstatement;
}

#endif /* A48E84EA_76E2_482D_A8B9_10441BB5D12D */
