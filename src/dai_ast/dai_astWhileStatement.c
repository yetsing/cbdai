#include <assert.h>

#include "dai_ast/dai_astWhileStatement.h"
#include "dai_ast/dai_astcommon.h"
#include "dai_malloc.h"
#include "dai_stringbuffer.h"

static void
DaiAstWhileStatement_free(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_WhileStatement);
    DaiAstWhileStatement* stmt = (DaiAstWhileStatement*)base;
    if (recursive) {
        if (stmt->condition != NULL) {
            stmt->condition->free_fn((DaiAstBase*)stmt->condition, true);
        }
        if (stmt->body != NULL) {
            stmt->body->free_fn((DaiAstBase*)stmt->body, true);
        }
    }
    dai_free(stmt);
}

static char*
DaiAstWhileStatement_string(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_WhileStatement);
    DaiAstWhileStatement* stmt = (DaiAstWhileStatement*)base;
    DaiStringBuffer*      sb   = DaiStringBuffer_New();
    DaiStringBuffer_write(sb, "{\n");
    DaiStringBuffer_write(sb, indent);
    // DaiStringBuffer_write(sb, "type: DaiAstType_WhileStatement,\n");
    DaiStringBuffer_write(sb, KEY_COLOR("type") ": " TYPE_COLOR("DaiAstType_WhileStatement") ",\n");

    DaiStringBuffer_write(sb, indent);
    DaiStringBuffer_write(sb, "condition: ");
    if (recursive) {
        char* s = stmt->condition->string_fn((DaiAstBase*)stmt->condition, recursive);
        DaiStringBuffer_writeWithLinePrefix(sb, s, indent);
        dai_free(s);
    } else {
        DaiStringBuffer_write(sb, indent);
        DaiStringBuffer_writePointer(sb, stmt->condition);
    }
    DaiStringBuffer_write(sb, ",\n");

    DaiStringBuffer_write(sb, indent);
    DaiStringBuffer_write(sb, "body: ");
    if (recursive) {
        char* s = stmt->body->string_fn((DaiAstBase*)stmt->body, recursive);
        DaiStringBuffer_writeWithLinePrefix(sb, s, indent);
        dai_free(s);
    } else {
        DaiStringBuffer_writeInt(sb, stmt->body->length);
    }
    DaiStringBuffer_write(sb, ",\n");
    DaiStringBuffer_write(sb, "}");
    char* s = DaiStringBuffer_getAndFree(sb, NULL);
    return s;
}

DaiAstWhileStatement*
DaiAstWhileStatement_New(void) {
    DaiAstWhileStatement* stmt = (DaiAstWhileStatement*)dai_malloc(sizeof(DaiAstWhileStatement));
    stmt->condition            = NULL;
    stmt->type                 = DaiAstType_WhileStatement;
    stmt->body                 = NULL;
    stmt->free_fn              = DaiAstWhileStatement_free;
    stmt->string_fn            = DaiAstWhileStatement_string;

    stmt->start_line   = 0;
    stmt->start_column = 0;
    stmt->end_line     = 0;
    stmt->end_column   = 0;
    return stmt;
}
