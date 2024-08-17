#ifndef B6EB20ED_104A_42E8_805B_F3EA16E68E74
#define B6EB20ED_104A_42E8_805B_F3EA16E68E74

#include "dai_parse/dai_parseStatementOfClass.h"

static DaiAstBlockStatement*
Parser_parseBlockStatementOfClass(Parser* p) {
    DaiAstBlockStatement* blockstatement = DaiAstBlockStatement_New();
    blockstatement->start_line           = p->cur_token->start_line;
    blockstatement->start_column         = p->cur_token->start_column;
    Parser_nextToken(p);

    while (!Parser_curTokenIs(p, DaiTokenType_rbrace) && !Parser_curTokenIs(p, DaiTokenType_eof)) {
        DaiAstStatement* stmt = Parser_parseStatementOfClass(p);
        if (stmt == NULL) {
            blockstatement->free_fn((DaiAstBase*)blockstatement, true);
            return NULL;
        }
        DaiAstBlockStatement_append(blockstatement, stmt);
        Parser_nextToken(p);
    }
    if (!Parser_curTokenIs(p, DaiTokenType_rbrace)) {
        int line   = p->cur_token->start_line;
        int column = p->cur_token->start_column;
        if (p->cur_token->type == DaiTokenType_eof) {
            line   = p->prev_token->end_line;
            column = p->prev_token->end_column;
        }
        p->syntax_error = DaiSyntaxError_New("expected '}'", line, column);
        blockstatement->free_fn((DaiAstBase*)blockstatement, true);
        return NULL;
    }
    blockstatement->start_line   = p->cur_token->start_line;
    blockstatement->start_column = p->cur_token->start_column;
    return blockstatement;
}

#endif /* B6EB20ED_104A_42E8_805B_F3EA16E68E74 */
