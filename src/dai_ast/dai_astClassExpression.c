#include <assert.h>

#include "dai_ast/dai_astClassExpression.h"
#include "dai_ast/dai_astcommon.h"
#include "dai_malloc.h"
#include "dai_stringbuffer.h"

static char*
DaiAstClassExpression_string(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_ClassExpression);
    DaiAstClassExpression* expr = (DaiAstClassExpression*)base;
    DaiStringBuffer* sb         = DaiStringBuffer_New();
    DaiStringBuffer_write(sb, "{\n");
    DaiStringBuffer_write(sb, indent);
    // DaiStringBuffer_write(sb, "type: DaiAstType_ClassExpression,\n");
    DaiStringBuffer_write(sb,
                          KEY_COLOR("type") ": " TYPE_COLOR("DaiAstType_ClassExpression") ",\n");

    if (expr->name != NULL) {
        DaiStringBuffer_write(sb, indent);
        DaiStringBuffer_write(sb, "name: ");
        {
            char* s = expr->name->string_fn((DaiAstBase*)expr->name, true);
            DaiStringBuffer_writeWithLinePrefix(sb, s, indent);
            dai_free(s);
        }
        DaiStringBuffer_write(sb, ",\n");
    }
    DaiStringBuffer_write(sb, "}");
    return DaiStringBuffer_getAndFree(sb, NULL);
}

static void
DaiAstClassExpression_free(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_ClassExpression);
    DaiAstClassExpression* expr = (DaiAstClassExpression*)base;
    if (recursive && expr->name != NULL) {
        expr->name->free_fn((DaiAstBase*)expr->name, true);
    }
    dai_free(expr);
}

static char*
DaiAstClassExpression_literal(DaiAstExpression* base) {
    assert(base->type == DaiAstType_ClassExpression);
    DaiAstClassExpression* expr = (DaiAstClassExpression*)base;
    DaiStringBuffer* sb         = DaiStringBuffer_New();
    DaiStringBuffer_write(sb, "(");
    DaiStringBuffer_write(sb, "class");
    if (expr->name != NULL) {
        DaiStringBuffer_writen(sb, ".", 1);
        char* s = expr->name->literal_fn((DaiAstExpression*)expr->name);
        DaiStringBuffer_write(sb, s);
        dai_free(s);
    }
    DaiStringBuffer_write(sb, ")");
    return DaiStringBuffer_getAndFree(sb, NULL);
}

DaiAstClassExpression*
DaiAstClassExpression_New(void) {
    DaiAstClassExpression* expr = (DaiAstClassExpression*)dai_malloc(sizeof(DaiAstClassExpression));
    expr->type                  = DaiAstType_ClassExpression;
    expr->name                  = NULL;
    {
        expr->string_fn  = DaiAstClassExpression_string;
        expr->free_fn    = DaiAstClassExpression_free;
        expr->literal_fn = DaiAstClassExpression_literal;
    }
    {
        expr->start_line   = 0;
        expr->start_column = 0;
        expr->end_line     = 0;
        expr->end_column   = 0;
    }
    return expr;
}
