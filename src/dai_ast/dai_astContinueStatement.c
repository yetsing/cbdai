#include <assert.h>

#include "dai_ast/dai_astContinueStatement.h"
#include "dai_ast/dai_astcommon.h"
#include "dai_malloc.h"
#include "dai_stringbuffer.h"

void
DaiAstContinueStatement_free(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_ContinueStatement);
    dai_free(base);
}

static char*
DaiAstContinueStatement_string(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_ContinueStatement);
    DaiStringBuffer* sb = DaiStringBuffer_New();
    DaiStringBuffer_write(sb, "{\n");
    DaiStringBuffer_write(sb, indent);
    // DaiStringBuffer_write(sb, "type: DaiAstType_ContinueStatement,\n");
    DaiStringBuffer_write(sb,
                          KEY_COLOR("type") ": " TYPE_COLOR("DaiAstType_ContinueStatement") ",\n");
    DaiStringBuffer_write(sb, "}");
    char* s = DaiStringBuffer_getAndFree(sb, NULL);
    return s;
}


DaiAstContinueStatement*
DaiAstContinueStatement_New(void) {
    DaiAstContinueStatement* stmt =
        (DaiAstContinueStatement*)dai_malloc(sizeof(DaiAstContinueStatement));
    stmt->type = DaiAstType_ContinueStatement;

    stmt->free_fn   = DaiAstContinueStatement_free;
    stmt->string_fn = DaiAstContinueStatement_string;

    stmt->start_line   = 0;
    stmt->start_column = 0;
    stmt->end_line     = 0;
    stmt->end_column   = 0;
    return stmt;
}