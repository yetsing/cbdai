#include <assert.h>
#include <string.h>

#include "dai_ast/dai_astcommon.h"
#include "dai_ast/dai_astnil.h"
#include "dai_malloc.h"
#include "dai_stringbuffer.h"

static char*
DaiAstNil_string(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_Nil);
    DaiStringBuffer* sb = DaiStringBuffer_New();
    DaiStringBuffer_write(sb, "{\n");
    DaiStringBuffer_write(sb, indent);
    // DaiStringBuffer_write(sb, "type: DaiAstType_Nil,\n");
    DaiStringBuffer_write(sb, KEY_COLOR("type") ": " TYPE_COLOR("DaiAstType_Nil") ",\n");
    DaiStringBuffer_write(sb, indent);
    DaiStringBuffer_write(sb, "value: nil");
    DaiStringBuffer_write(sb, ",\n");
    DaiStringBuffer_write(sb, "}");
    return DaiStringBuffer_getAndFree(sb, NULL);
}

static void
DaiAstNil_free(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_Nil);
    DaiAstNil* lit = (DaiAstNil*)base;
    dai_free(lit);
}

static char*
DaiAstNil_literal(DaiAstExpression* expr) {
    assert(expr->type == DaiAstType_Nil);
    return strdup("nil");
}


DaiAstNil*
DaiAstNil_New(DaiToken* token) {
    DaiAstNil* boolean = dai_malloc(sizeof(DaiAstNil));
    {
        boolean->type       = DaiAstType_Nil;
        boolean->string_fn  = DaiAstNil_string;
        boolean->free_fn    = DaiAstNil_free;
        boolean->literal_fn = DaiAstNil_literal;
    }
    boolean->start_line   = token->start_line;
    boolean->start_column = token->start_column;
    boolean->end_line     = token->end_line;
    boolean->end_column   = token->end_column;
    return boolean;
}