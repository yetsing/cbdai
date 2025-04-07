#include <assert.h>

#include "dai_ast/dai_astForInStatement.h"
#include "dai_ast/dai_astcommon.h"
#include "dai_malloc.h"
#include "dai_stringbuffer.h"

static void
DaiAstForInStatement_free(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_ForInStatement);
    DaiAstForInStatement* stmt = (DaiAstForInStatement*)base;
    if (recursive) {
        if (stmt->i != NULL) {
            stmt->i->free_fn((DaiAstBase*)stmt->i, true);
        }
        if (stmt->e != NULL) {
            stmt->e->free_fn((DaiAstBase*)stmt->e, true);
        }
        if (stmt->expression != NULL) {
            stmt->expression->free_fn((DaiAstBase*)stmt->expression, true);
        }
        if (stmt->body != NULL) {
            stmt->body->free_fn((DaiAstBase*)stmt->body, true);
        }
    }
    dai_free(stmt);
}

static char*
DaiAstForInStatement_string(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_ForInStatement);
    DaiAstForInStatement* stmt = (DaiAstForInStatement*)base;
    DaiStringBuffer* sb        = DaiStringBuffer_New();
    DaiStringBuffer_write(sb, "{\n");
    DaiStringBuffer_write(sb, indent);
    // DaiStringBuffer_write(sb, "type: DaiAstType_ForInStatement,\n");
    DaiStringBuffer_write(sb, KEY_COLOR("type") ": " TYPE_COLOR("DaiAstType_ForInStatement") ",\n");

    DaiStringBuffer_write(sb, indent);
    if (stmt->i) {
        DaiStringBuffer_writef(sb, "var: %s, %s,\n", stmt->i->value, stmt->e->value);
    } else {
        DaiStringBuffer_writef(sb, "var: _, %s,\n", stmt->e->value);
    }

    DaiStringBuffer_write(sb, indent);
    DaiStringBuffer_write(sb, "iterator: ");
    if (recursive) {
        char* s = stmt->expression->string_fn((DaiAstBase*)stmt->expression, recursive);
        DaiStringBuffer_writeWithLinePrefix(sb, s, indent);
        dai_free(s);
    } else {
        DaiStringBuffer_write(sb, indent);
        DaiStringBuffer_writePointer(sb, stmt->expression);
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

DaiAstForInStatement*
DaiAstForInStatement_New(void) {
    DaiAstForInStatement* stmt = (DaiAstForInStatement*)dai_malloc(sizeof(DaiAstForInStatement));
    stmt->i                    = NULL;
    stmt->e                    = NULL;
    stmt->expression           = NULL;
    stmt->type                 = DaiAstType_ForInStatement;
    stmt->body                 = NULL;
    stmt->free_fn              = DaiAstForInStatement_free;
    stmt->string_fn            = DaiAstForInStatement_string;

    stmt->start_line   = 0;
    stmt->start_column = 0;
    stmt->end_line     = 0;
    stmt->end_column   = 0;
    return stmt;
}
