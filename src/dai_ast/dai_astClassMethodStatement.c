#include <assert.h>

#include "dai_array.h"
#include "dai_ast/dai_astClassMethodStatement.h"
#include "dai_ast/dai_astcommon.h"
#include "dai_malloc.h"
#include "dai_stringbuffer.h"

static char*
DaiAstClassMethodStatement_string(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_ClassMethodStatement);
    DaiAstClassMethodStatement* stmt = (DaiAstClassMethodStatement*)base;
    DaiStringBuffer* sb              = DaiStringBuffer_New();
    DaiStringBuffer_write(sb, "{\n");
    DaiStringBuffer_write(sb, indent);
    // DaiStringBuffer_write(sb, "type: DaiAstType_ClassMethodStatement,\n");
    DaiStringBuffer_write(
        sb, KEY_COLOR("type") ": " TYPE_COLOR("DaiAstType_ClassMethodStatement") ",\n");
    DaiStringBuffer_writef(sb, "%sname: %s,\n", indent, stmt->name);
    {
        DaiStringBuffer_write(sb, indent);
        DaiStringBuffer_write(sb, "parameters: [\n");
        size_t default_length = DaiArray_length(stmt->defaults);
        for (size_t i = 0; i < stmt->parameters_count; i++) {
            DaiStringBuffer_write(sb, doubleindent);
            DaiAstIdentifier* param = stmt->parameters[i];
            DaiStringBuffer_write(sb, param->value);
            if (i >= stmt->parameters_count - default_length) {
                DaiStringBuffer_write(sb, " = ");
                DaiAstExpression* e = *(DaiAstExpression**)DaiArray_get(
                    stmt->defaults, i - (stmt->parameters_count - default_length));
                if (recursive) {
                    char* s = e->string_fn((DaiAstBase*)e, recursive);
                    DaiStringBuffer_writeWithLinePrefix(sb, s, doubleindent);
                    dai_free(s);
                } else {
                    DaiStringBuffer_writePointer(sb, e);
                }
            }
            DaiStringBuffer_write(sb, ",\n");
        }
        DaiStringBuffer_write(sb, indent);
        DaiStringBuffer_write(sb, "],\n");
    }

    {
        DaiStringBuffer_write(sb, indent);
        DaiStringBuffer_write(sb, "body: ");
        if (recursive) {
            char* s = stmt->body->string_fn((DaiAstBase*)stmt->body, recursive);
            DaiStringBuffer_writeWithLinePrefix(sb, s, indent);
            dai_free(s);
        } else {
            DaiStringBuffer_writePointer(sb, stmt->body);
        }
        DaiStringBuffer_write(sb, ",\n");
    }
    DaiStringBuffer_write(sb, "}");
    char* s = DaiStringBuffer_getAndFree(sb, NULL);
    return s;
}

static void
DaiAstClassMethodStatement_free(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_ClassMethodStatement);
    DaiAstClassMethodStatement* stmt = (DaiAstClassMethodStatement*)base;
    if (stmt->parameters != NULL) {
        if (recursive) {
            for (size_t i = 0; i < stmt->parameters_count; i++) {
                stmt->parameters[i]->free_fn((DaiAstBase*)stmt->parameters[i], true);
            }
        }
        dai_free(stmt->parameters);
    }
    if (stmt->defaults != NULL) {
        if (recursive) {
            for (size_t i = 0; i < DaiArray_length(stmt->defaults); i++) {
                DaiAstExpression* e = *(DaiAstExpression**)DaiArray_get(stmt->defaults, i);
                e->free_fn((DaiAstBase*)e, true);
            }
        }
        DaiArray_free(stmt->defaults);
    }
    if (stmt->body != NULL) {
        if (recursive) {
            stmt->body->free_fn((DaiAstBase*)stmt->body, true);
        }
    }
    dai_free(stmt->name);
    dai_free(base);
}

DaiAstClassMethodStatement*
DaiAstClassMethodStatement_New(void) {
    DaiAstClassMethodStatement* f = dai_malloc(sizeof(DaiAstClassMethodStatement));
    f->name                       = NULL;
    f->parameters_count           = 0;
    f->parameters                 = NULL;
    f->defaults                   = DaiArray_New(sizeof(DaiAstExpression*));
    f->body                       = NULL;
    {
        f->type      = DaiAstType_ClassMethodStatement;
        f->free_fn   = DaiAstClassMethodStatement_free;
        f->string_fn = DaiAstClassMethodStatement_string;
    }
    {
        f->start_line   = 0;
        f->start_column = 0;
        f->end_line     = 0;
        f->end_column   = 0;
    }
    return f;
}
