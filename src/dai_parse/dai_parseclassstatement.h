#ifndef ECDAF7F1_FB0A_493B_B01B_BE17C9AC275F
#define ECDAF7F1_FB0A_493B_B01B_BE17C9AC275F

#include "dai_parse/dai_parseBlockStatementOfClass.h"

static DaiAstClassStatement* Parser_parseClassStatement(Parser* p) {
    DaiToken* start_token = p->cur_token;
    // 类名
    if (!Parser_expectPeek(p, DaiTokenType_ident)) {
        return NULL;
    }
    DaiAstClassStatement* klass = DaiAstClassStatement_New(p->cur_token);
    {
        klass->start_line = start_token->start_line;
        klass->start_column = start_token->start_column;
    }
    if (Parser_peekTokenIs(p, DaiTokenType_lt)) {
        // 父类
        Parser_nextToken(p);
        Parser_nextToken(p);
        klass->parent = (DaiAstIdentifier *) Parser_parseIdentifier(p);
        if (klass->parent == NULL) {
            klass->free_fn((DaiAstBase*)klass, true);
            return NULL;
        }
    }

    if (!Parser_expectPeek(p, DaiTokenType_lbrace)) {
        klass->free_fn((DaiAstBase*)klass, true);
        return NULL;
    }
    DaiAstBlockStatement* body = Parser_parseBlockStatementOfClass(p);
    if (body == NULL) {
        klass->free_fn((DaiAstBase*)klass, true);
        return NULL;
    }
    klass->body = body;

    if (!Parser_expectPeek(p, DaiTokenType_semicolon)) {
        klass->free_fn((DaiAstBase*)klass, true);
        return NULL;
    }
    {
        klass->end_line = p->cur_token->end_line;
        klass->end_column = p->cur_token->end_column;
    }
    return klass;
}

#endif /* ECDAF7F1_FB0A_493B_B01B_BE17C9AC275F */