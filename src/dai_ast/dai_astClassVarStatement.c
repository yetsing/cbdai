#include <assert.h>

#include "dai_ast/dai_astClassVarStatement.h"
#include "dai_ast/dai_astcommon.h"
#include "dai_malloc.h"
#include "dai_stringbuffer.h"

static char*
DaiAstClassVarStatement_string(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_ClassVarStatement);
    DaiAstClassVarStatement* stmt = (DaiAstClassVarStatement*)base;
    DaiStringBuffer*         sb   = DaiStringBuffer_New();
    DaiStringBuffer_write(sb, "{\n");
    DaiStringBuffer_write(sb, indent);
    // DaiStringBuffer_write(sb, "type: DaiAstType_ClassVarStatement,\n");
    DaiStringBuffer_write(sb,
                          KEY_COLOR("type") ": " TYPE_COLOR("DaiAstType_ClassVarStatement") ",\n");
    DaiStringBuffer_write(sb, indent);
    DaiStringBuffer_write(sb, "name: ");
    DaiStringBuffer_write(sb, stmt->name->value);
    DaiStringBuffer_write(sb, ",\n");
    if (stmt->value != NULL) {
        if (recursive) {
            DaiStringBuffer_write(sb, indent);
            DaiStringBuffer_write(sb, "value: ");
            char* value = stmt->value->string_fn((DaiAstBase*)stmt->value, true);
            DaiStringBuffer_writeWithLinePrefix(sb, value, indent);
            DaiStringBuffer_write(sb, ",\n");
            dai_free(value);
        } else {
            DaiStringBuffer_write(sb, indent);
            DaiStringBuffer_write(sb, "value: ");
            DaiStringBuffer_writePointer(sb, stmt->value);
            DaiStringBuffer_write(sb, ",\n");
        }
    }

    DaiStringBuffer_write(sb, "}");
    return DaiStringBuffer_getAndFree(sb, NULL);
}

static void
DaiAstClassVarStatement_free(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_ClassVarStatement);
    DaiAstClassVarStatement* stmt = (DaiAstClassVarStatement*)base;
    if (recursive) {
        if (stmt->name != NULL) {
            stmt->name->free_fn((DaiAstBase*)stmt->name, true);
        }
        if (stmt->value != NULL) {
            stmt->value->free_fn((DaiAstBase*)stmt->value, true);
        }
    }
    dai_free(stmt);
}

DaiAstClassVarStatement*
DaiAstClassVarStatement_New(DaiAstIdentifier* name, DaiAstExpression* value) {
    DaiAstClassVarStatement* stmt = dai_malloc(sizeof(DaiAstClassVarStatement));
    {
        stmt->type      = DaiAstType_ClassVarStatement;
        stmt->string_fn = DaiAstClassVarStatement_string;
        stmt->free_fn   = DaiAstClassVarStatement_free;
    }
    stmt->name  = name;
    stmt->value = value;
    return stmt;
}
