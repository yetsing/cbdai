#include <assert.h>

#include "dai_ast/dai_astBreakStatement.h"
#include "dai_ast/dai_astcommon.h"
#include "dai_malloc.h"
#include "dai_stringbuffer.h"

static void
DaiAstBreakStatement_free(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_BreakStatement);
    dai_free(base);
}

static char*
DaiAstBreakStatement_string(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_BreakStatement);
    DaiStringBuffer* sb = DaiStringBuffer_New();
    DaiStringBuffer_write(sb, "{\n");
    DaiStringBuffer_write(sb, indent);
    // DaiStringBuffer_write(sb, "type: DaiAstType_BreakStatement,\n");
    DaiStringBuffer_write(sb, KEY_COLOR("type") ": " TYPE_COLOR("DaiAstType_BreakStatement") ",\n");
    DaiStringBuffer_write(sb, "}");
    char* s = DaiStringBuffer_getAndFree(sb, NULL);
    return s;
}


DaiAstBreakStatement*
DaiAstBreakStatement_New(void) {
    DaiAstBreakStatement* stmt = (DaiAstBreakStatement*)dai_malloc(sizeof(DaiAstBreakStatement));
    stmt->type                 = DaiAstType_BreakStatement;

    stmt->free_fn   = DaiAstBreakStatement_free;
    stmt->string_fn = DaiAstBreakStatement_string;

    stmt->start_line   = 0;
    stmt->start_column = 0;
    stmt->end_line     = 0;
    stmt->end_column   = 0;
    return stmt;
}