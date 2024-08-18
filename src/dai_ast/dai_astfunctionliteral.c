#include <assert.h>

#include "dai_ast/dai_astcommon.h"
#include "dai_ast/dai_astfunctionliteral.h"
#include "dai_malloc.h"
#include "dai_stringbuffer.h"

static char*
DaiAstFunctionLiteral_string(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_FunctionLiteral);
    DaiAstFunctionLiteral* expr = (DaiAstFunctionLiteral*)base;
    DaiStringBuffer* sb         = DaiStringBuffer_New();
    DaiStringBuffer_write(sb, "{\n");
    DaiStringBuffer_write(sb, indent);
    // DaiStringBuffer_write(sb, "type: DaiAstType_FunctionLiteral,\n");
    DaiStringBuffer_write(sb,
                          KEY_COLOR("type") ": " TYPE_COLOR("DaiAstType_FunctionLiteral") ",\n");
    {
        DaiStringBuffer_write(sb, indent);
        DaiStringBuffer_write(sb, "parameters: [");
        for (size_t i = 0; i < expr->parameters_count; i++) {
            DaiAstIdentifier* param = expr->parameters[i];
            DaiStringBuffer_write(sb, param->value);
            DaiStringBuffer_write(sb, ", ");
        }
        DaiStringBuffer_write(sb, "],\n");
    }

    {
        DaiStringBuffer_write(sb, indent);
        DaiStringBuffer_write(sb, "body: ");
        if (recursive) {
            char* s = expr->body->string_fn((DaiAstBase*)expr->body, recursive);
            DaiStringBuffer_writeWithLinePrefix(sb, s, indent);
            dai_free(s);
        } else {
            DaiStringBuffer_writePointer(sb, expr->body);
        }
        DaiStringBuffer_write(sb, ",\n");
    }
    DaiStringBuffer_write(sb, "}");
    char* s = DaiStringBuffer_getAndFree(sb, NULL);
    return s;
}

static void
DaiAstFunctionLiteral_free(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_FunctionLiteral);
    DaiAstFunctionLiteral* expr = (DaiAstFunctionLiteral*)base;
    if (expr->parameters != NULL) {
        if (recursive) {
            for (size_t i = 0; i < expr->parameters_count; i++) {
                expr->parameters[i]->free_fn((DaiAstBase*)expr->parameters[i], true);
            }
        }
        dai_free(expr->parameters);
    }
    if (expr->body != NULL) {
        if (recursive) {
            expr->body->free_fn((DaiAstBase*)expr->body, true);
        }
    }
    dai_free(base);
}

static char*
DaiAstFunctionLiteral_literal(DaiAstExpression* base) {
    assert(base->type == DaiAstType_FunctionLiteral);
    DaiAstFunctionLiteral* expr = (DaiAstFunctionLiteral*)base;
    DaiStringBuffer* sb         = DaiStringBuffer_New();
    DaiStringBuffer_write(sb, "fn(");
    for (size_t i = 0; i < expr->parameters_count; i++) {
        DaiAstIdentifier* param = expr->parameters[i];
        DaiStringBuffer_write(sb, param->value);
        DaiStringBuffer_write(sb, ", ");
    }

    DaiStringBuffer_write(sb, ")");
    DaiStringBuffer_write(sb, "{...}");
    return DaiStringBuffer_getAndFree(sb, NULL);
}

DaiAstFunctionLiteral*
DaiAstFunctionLiteral_New(void) {
    DaiAstFunctionLiteral* f = dai_malloc(sizeof(DaiAstFunctionLiteral));
    f->parameters_count      = 0;
    f->parameters            = NULL;
    f->body                  = NULL;
    {
        f->type       = DaiAstType_FunctionLiteral;
        f->free_fn    = DaiAstFunctionLiteral_free;
        f->string_fn  = DaiAstFunctionLiteral_string;
        f->literal_fn = DaiAstFunctionLiteral_literal;
    }
    {
        f->start_line   = 0;
        f->start_column = 0;
        f->end_line     = 0;
        f->end_column   = 0;
    }
    return f;
}
