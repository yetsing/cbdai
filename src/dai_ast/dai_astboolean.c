#include <assert.h>
#include <string.h>

#include "dai_ast/dai_astboolean.h"
#include "dai_ast/dai_astcommon.h"
#include "dai_malloc.h"
#include "dai_stringbuffer.h"

static char*
DaiAstBoolean_string(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_Boolean);
    DaiAstBoolean*   boolean = (DaiAstBoolean*)base;
    DaiStringBuffer* sb      = DaiStringBuffer_New();
    DaiStringBuffer_write(sb, "{\n");
    DaiStringBuffer_write(sb, indent);
    // DaiStringBuffer_write(sb, "type: DaiAstType_Boolean,\n");
    DaiStringBuffer_write(sb, KEY_COLOR("type") ": " TYPE_COLOR("DaiAstType_Boolean") ",\n");
    DaiStringBuffer_write(sb, indent);
    DaiStringBuffer_write(sb, "value: ");
    DaiStringBuffer_write(sb, boolean->value ? "true" : "false");
    DaiStringBuffer_write(sb, ",\n");
    DaiStringBuffer_write(sb, "}");
    return DaiStringBuffer_getAndFree(sb, NULL);
}

static void
DaiAstBoolean_free(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_Boolean);
    DaiAstBoolean* boolean = (DaiAstBoolean*)base;
    dai_free(boolean);
}

static char*
DaiAstBoolean_literal(DaiAstExpression* expr) {
    assert(expr->type == DaiAstType_Boolean);
    DaiAstBoolean* boolean = (DaiAstBoolean*)expr;
    char*          s       = boolean->value ? "true" : "false";
    return strdup(s);
}


DaiAstBoolean*
DaiAstBoolean_New(DaiToken* token) {
    DaiAstBoolean* boolean = dai_malloc(sizeof(DaiAstBoolean));
    {
        boolean->type       = DaiAstType_Boolean;
        boolean->string_fn  = DaiAstBoolean_string;
        boolean->free_fn    = DaiAstBoolean_free;
        boolean->literal_fn = DaiAstBoolean_literal;
    }
    boolean->value        = strcmp(token->literal, "true") == 0;
    boolean->start_line   = token->start_line;
    boolean->start_column = token->start_column;
    boolean->end_line     = token->end_line;
    boolean->end_column   = token->end_column;
    return boolean;
}