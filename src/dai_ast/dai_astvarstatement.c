//
// Created by  on 2024/6/5.
//
#include <assert.h>

#include "dai_ast/dai_astcommon.h"
#include "dai_astvarstatement.h"
#include "dai_malloc.h"
#include "dai_stringbuffer.h"

static char*
DaiAstVarStatement_string(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_VarStatement);
    DaiAstVarStatement* stmt = (DaiAstVarStatement*)base;
    DaiStringBuffer* sb      = DaiStringBuffer_New();
    DaiStringBuffer_write(sb, "{\n");
    DaiStringBuffer_write(sb, indent);
    // DaiStringBuffer_write(sb, "type: DaiAstType_VarStatement,\n");
    DaiStringBuffer_write(sb, KEY_COLOR("type") ": " TYPE_COLOR("DaiAstType_VarStatement") ",\n");
    DaiStringBuffer_write(sb, indent);
    DaiStringBuffer_write(sb, "is_con: ");
    DaiStringBuffer_write(sb, stmt->is_con ? "true" : "false");
    DaiStringBuffer_write(sb, ",\n");
    DaiStringBuffer_write(sb, indent);
    DaiStringBuffer_write(sb, "name: ");
    DaiStringBuffer_write(sb, stmt->name->value);
    DaiStringBuffer_write(sb, ",\n");
    if (recursive) {
        DaiStringBuffer_write(sb, indent);
        DaiStringBuffer_write(sb, "value: ");
        char* value = stmt->value->string_fn((DaiAstBase*)stmt->value, true);
        DaiStringBuffer_writeWithLinePrefix(sb, value, indent);
        DaiStringBuffer_write(sb, ",\n");
        dai_free(value);
    } else {
        DaiStringBuffer_write(sb, "    value: ");
        DaiStringBuffer_writePointer(sb, stmt->value);
        DaiStringBuffer_write(sb, ",\n");
    }

    DaiStringBuffer_write(sb, "}");
    return DaiStringBuffer_getAndFree(sb, NULL);
}

static void
DaiAstVarStatement_free(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_VarStatement);
    DaiAstVarStatement* stmt = (DaiAstVarStatement*)base;
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

DaiAstVarStatement*
DaiAstVarStatement_New(DaiAstIdentifier* name, DaiAstExpression* value) {
    DaiAstVarStatement* stmt = dai_malloc(sizeof(DaiAstVarStatement));
    {
        stmt->type      = DaiAstType_VarStatement;
        stmt->string_fn = DaiAstVarStatement_string;
        stmt->free_fn   = DaiAstVarStatement_free;
    }
    stmt->is_con = false;
    stmt->name   = name;
    stmt->value  = value;
    return stmt;
}
