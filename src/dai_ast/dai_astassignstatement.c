#include <assert.h>

#include "dai_ast/dai_astassignstatement.h"
#include "dai_ast/dai_astcommon.h"
#include "dai_malloc.h"
#include "dai_stringbuffer.h"

char*
DaiAstAssignStatement_string(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_AssignStatement);
    DaiAstAssignStatement* stmt = (DaiAstAssignStatement*)base;
    DaiStringBuffer* sb         = DaiStringBuffer_New();
    DaiStringBuffer_write(sb, "{\n");
    DaiStringBuffer_write(sb, indent);
    // DaiStringBuffer_write(sb, "type: DaiAstType_AssignStatement,\n");
    DaiStringBuffer_write(sb,
                          KEY_COLOR("type") ": " TYPE_COLOR("DaiAstType_AssignStatement") ",\n");
    DaiStringBuffer_write(sb, indent);
    DaiStringBuffer_write(sb, "left: ");
    if (recursive) {
        char* left = stmt->left->string_fn((DaiAstBase*)stmt->left, true);
        DaiStringBuffer_writeWithLinePrefix(sb, left, indent);
        dai_free(left);
    } else {
        DaiStringBuffer_writePointer(sb, stmt->left);
    }
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

void
DaiAstAssignStatement_free(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_AssignStatement);
    DaiAstAssignStatement* stmt = (DaiAstAssignStatement*)base;
    if (recursive) {
        if (stmt->left != NULL) {
            stmt->left->free_fn((DaiAstBase*)stmt->left, true);
        }
        if (stmt->value != NULL) {
            stmt->value->free_fn((DaiAstBase*)stmt->value, true);
        }
    }
    dai_free(stmt);
}

DaiAstAssignStatement*
DaiAstAssignStatement_New(void) {
    DaiAstAssignStatement* stmt = (DaiAstAssignStatement*)dai_malloc(sizeof(DaiAstAssignStatement));
    {
        stmt->string_fn = DaiAstAssignStatement_string;
        stmt->free_fn   = DaiAstAssignStatement_free;
    }
    stmt->type  = DaiAstType_AssignStatement;
    stmt->left  = NULL;
    stmt->value = NULL;
    return stmt;
}