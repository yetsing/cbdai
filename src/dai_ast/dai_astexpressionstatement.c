//
// Created by  on 2024/6/5.
//
#include <assert.h>

#include "dai_ast/dai_astcommon.h"
#include "dai_ast/dai_astexpressionstatement.h"
#include "dai_malloc.h"
#include "dai_stringbuffer.h"

static char*
DaiAstExpressionStatement_string(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_ExpressionStatement);
    DaiAstExpressionStatement* stmt = (DaiAstExpressionStatement*)base;
    DaiStringBuffer* sb             = DaiStringBuffer_New();
    DaiStringBuffer_write(sb, "{\n");
    DaiStringBuffer_write(sb, indent);
    // DaiStringBuffer_write(sb, "type: DaiAstType_ExpressionStatement,\n");
    DaiStringBuffer_write(
        sb, KEY_COLOR("type") ": " TYPE_COLOR("DaiAstType_ExpressionStatement") ",\n");
    DaiStringBuffer_write(sb, indent);
    DaiStringBuffer_write(sb, "expression: ");
    if (recursive) {
        char* value = stmt->expression->string_fn((DaiAstBase*)stmt->expression, true);
        DaiStringBuffer_writeWithLinePrefix(sb, value, indent);
        dai_free(value);
    } else {
        DaiStringBuffer_writePointer(sb, stmt->expression);
    }
    DaiStringBuffer_write(sb, ",\n");
    DaiStringBuffer_write(sb, "}");
    return DaiStringBuffer_getAndFree(sb, NULL);
}

static void
DaiAstExpressionStatement_free(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_ExpressionStatement);
    DaiAstExpressionStatement* stmt = (DaiAstExpressionStatement*)base;
    if (recursive) {
        stmt->expression->free_fn((DaiAstBase*)stmt->expression, true);
    }
    dai_free(stmt);
}

DaiAstExpressionStatement*
DaiAstExpressionStatement_New(DaiAstExpression* expression) {
    DaiAstExpressionStatement* stmt = dai_malloc(sizeof(DaiAstExpressionStatement));
    {
        stmt->type      = DaiAstType_ExpressionStatement;
        stmt->string_fn = DaiAstExpressionStatement_string;
        stmt->free_fn   = DaiAstExpressionStatement_free;
    }
    stmt->expression = expression;
    return stmt;
}
