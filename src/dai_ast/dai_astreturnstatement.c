//
// Created by  on 2024/6/5.
//
#include <assert.h>

#include "dai_ast/dai_astcommon.h"
#include "dai_ast/dai_astreturnstatement.h"
#include "dai_malloc.h"
#include "dai_stringbuffer.h"

static char*
DaiAstReturnStatement_string(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_ReturnStatement);
    DaiAstReturnStatement* stmt = (DaiAstReturnStatement*)base;
    DaiStringBuffer* sb         = DaiStringBuffer_New();
    DaiStringBuffer_write(sb, "{\n");
    DaiStringBuffer_write(sb, indent);
    // DaiStringBuffer_write(sb, "type: DaiAstType_ReturnStatement,\n");
    DaiStringBuffer_write(sb,
                          KEY_COLOR("type") ": " TYPE_COLOR("DaiAstType_ReturnStatement") ",\n");
    DaiStringBuffer_write(sb, indent);
    DaiStringBuffer_write(sb, "return_value: ");
    if (recursive) {
        char* value = stmt->return_value->string_fn((DaiAstBase*)stmt->return_value, true);
        DaiStringBuffer_writeWithLinePrefix(sb, value, indent);
        dai_free(value);
    } else {
        DaiStringBuffer_writePointer(sb, stmt->return_value);
    }
    DaiStringBuffer_write(sb, ",\n");
    DaiStringBuffer_write(sb, "}");
    return DaiStringBuffer_getAndFree(sb, NULL);
}

static void
DaiAstReturnStatement_free(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_ReturnStatement);
    DaiAstReturnStatement* stmt = (DaiAstReturnStatement*)base;
    if (recursive && stmt->return_value != NULL) {
        stmt->return_value->free_fn((DaiAstBase*)stmt->return_value, true);
    }
    dai_free(stmt);
}

DaiAstReturnStatement*
DaiAstReturnStatement_New(DaiAstExpression* return_value) {
    DaiAstReturnStatement* stmt = dai_malloc(sizeof(DaiAstReturnStatement));
    {
        stmt->type      = DaiAstType_ReturnStatement;
        stmt->string_fn = DaiAstReturnStatement_string;
        stmt->free_fn   = DaiAstReturnStatement_free;
    }
    stmt->return_value = return_value;
    return stmt;
}
