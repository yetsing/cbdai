#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "dai_ast/dai_astcommon.h"
#include "dai_ast/dai_astidentifier.h"
#include "dai_malloc.h"
#include "dai_stringbuffer.h"

char*
DaiAstIdentifier_string(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_Identifier);
    DaiAstIdentifier* id = (DaiAstIdentifier*)base;
    DaiStringBuffer*  sb = DaiStringBuffer_New();
    DaiStringBuffer_write(sb, "{\n");
    DaiStringBuffer_write(sb, indent);
    // DaiStringBuffer_write(sb, "type: DaiAstType_Identifier,\n");
    DaiStringBuffer_write(sb, KEY_COLOR("type") ": " TYPE_COLOR("DaiAstType_Identifier") ",\n");
    DaiStringBuffer_write(sb, indent);
    DaiStringBuffer_write(sb, "value: ");
    DaiStringBuffer_write(sb, GREEN);
    DaiStringBuffer_writen(sb, id->value, strlen(id->value));
    DaiStringBuffer_write(sb, RESET);
    DaiStringBuffer_write(sb, ",\n");
    DaiStringBuffer_write(sb, "}");
    return DaiStringBuffer_getAndFree(sb, NULL);
}

static void
DaiAstIdentifier_free(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_Identifier);
    DaiAstIdentifier* id = (DaiAstIdentifier*)base;
    dai_free(id->value);
    dai_free(id);
}

static char*
DaiAstIdentifier_literal(DaiAstExpression* expr) {
    assert(expr->type == DaiAstType_Identifier);
    DaiAstIdentifier* id = (DaiAstIdentifier*)expr;
    return strdup(id->value);
}

DaiAstIdentifier*
DaiAstIdentifier_New(DaiToken* token) {
    DaiAstIdentifier* id = dai_malloc(sizeof(DaiAstIdentifier));
    {
        id->type       = DaiAstType_Identifier;
        id->string_fn  = DaiAstIdentifier_string;
        id->free_fn    = DaiAstIdentifier_free;
        id->literal_fn = DaiAstIdentifier_literal;
    }
    id->value        = strdup(token->literal);
    id->start_line   = token->start_line;
    id->start_column = token->start_column;
    id->end_line     = token->end_line;
    id->end_column   = token->end_column;
    return id;
}
