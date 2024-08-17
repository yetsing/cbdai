#include <assert.h>

#include "dai_ast/dai_astcallexpression.h"
#include "dai_ast/dai_astcommon.h"
#include "dai_malloc.h"
#include "dai_stringbuffer.h"

static char*
DaiAstCallExpression_string(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_CallExpression);
    DaiAstCallExpression* call = (DaiAstCallExpression*)base;
    DaiStringBuffer*      sb   = DaiStringBuffer_New();
    DaiStringBuffer_write(sb, "{\n");
    DaiStringBuffer_write(sb, indent);
    // DaiStringBuffer_write(sb, "type: DaiAstType_CallExpression,\n");
    DaiStringBuffer_write(sb, KEY_COLOR("type") ": " TYPE_COLOR("DaiAstType_CallExpression") ",\n");
    {
        DaiStringBuffer_write(sb, indent);
        DaiStringBuffer_write(sb, "function: ");
        if (recursive) {
            char* s = call->function->string_fn((DaiAstBase*)call->function, true);
            DaiStringBuffer_writeWithLinePrefix(sb, s, indent);
            dai_free(s);
        } else {
            DaiStringBuffer_writePointer(sb, call->function);
        }
        DaiStringBuffer_write(sb, ",\n");
    }
    {
        DaiStringBuffer_write(sb, indent);
        DaiStringBuffer_write(sb, "arguments: [\n");
        if (recursive) {
            for (size_t i = 0; i < call->arguments_count; i++) {
                char* s = call->arguments[i]->string_fn((DaiAstBase*)call->arguments[i], true);
                DaiStringBuffer_writeWithLinePrefix(sb, s, doubleindent);
                dai_free(s);
                DaiStringBuffer_write(sb, ",\n");
            }
        } else {
            DaiStringBuffer_writen(sb, "<", 1);
            DaiStringBuffer_writeInt64(sb, call->arguments_count);
            DaiStringBuffer_writen(sb, ">", 1);
        }
        DaiStringBuffer_write(sb, indent);
        DaiStringBuffer_write(sb, "],\n");
    }
    DaiStringBuffer_write(sb, "}");
    return DaiStringBuffer_getAndFree(sb, NULL);
}

static void
DaiAstCallExpression_free(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_CallExpression);
    DaiAstCallExpression* call = (DaiAstCallExpression*)base;
    if (recursive) {
        call->function->free_fn((DaiAstBase*)call->function, true);
        if (call->arguments != NULL) {
            for (size_t i = 0; i < call->arguments_count; i++) {
                call->arguments[i]->free_fn((DaiAstBase*)call->arguments[i], true);
            }
        }
    }
    dai_free(call->arguments);
    dai_free(call);
}

static char*
DaiAstCallExpression_literal(DaiAstExpression* base) {
    assert(base->type == DaiAstType_CallExpression);
    DaiAstCallExpression* call = (DaiAstCallExpression*)base;
    DaiStringBuffer*      sb   = DaiStringBuffer_New();
    {
        char* s = call->function->literal_fn(call->function);
        DaiStringBuffer_write(sb, s);
        dai_free(s);
    }
    DaiStringBuffer_writen(sb, "(", 1);
    {
        for (size_t i = 0; i < call->arguments_count; i++) {
            char* s = call->arguments[i]->literal_fn(call->arguments[i]);
            DaiStringBuffer_write(sb, s);
            dai_free(s);
            if (i < call->arguments_count - 1) {
                DaiStringBuffer_write(sb, ", ");
            }
        }
    }
    DaiStringBuffer_writen(sb, ")", 1);
    return DaiStringBuffer_getAndFree(sb, NULL);
}

DaiAstCallExpression*
DaiAstCallExpression_New(void) {
    DaiAstCallExpression* call = (DaiAstCallExpression*)dai_malloc(sizeof(DaiAstCallExpression));
    call->type                 = DaiAstType_CallExpression;
    {
        call->free_fn    = DaiAstCallExpression_free;
        call->string_fn  = DaiAstCallExpression_string;
        call->literal_fn = DaiAstCallExpression_literal;
    }
    call->function  = NULL;
    call->arguments = NULL;
    {
        call->start_line   = 0;
        call->start_column = 0;
        call->end_line     = 0;
        call->end_column   = 0;
    }
    return call;
}