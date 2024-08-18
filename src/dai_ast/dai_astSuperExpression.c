#include <assert.h>

#include "dai_ast/dai_astSuperExpression.h"
#include "dai_ast/dai_astcommon.h"
#include "dai_malloc.h"
#include "dai_stringbuffer.h"

static char*
DaiAstSuperExpression_string(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_SuperExpression);
    DaiAstSuperExpression* expr = (DaiAstSuperExpression*)base;
    DaiStringBuffer* sb         = DaiStringBuffer_New();
    DaiStringBuffer_write(sb, "{\n");
    DaiStringBuffer_write(sb, indent);
    // DaiStringBuffer_write(sb, "type: DaiAstType_SuperExpression,\n");
    DaiStringBuffer_write(sb,
                          KEY_COLOR("type") ": " TYPE_COLOR("DaiAstType_SuperExpression") ",\n");

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
DaiAstSuperExpression_free(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_SuperExpression);
    DaiAstSuperExpression* expr = (DaiAstSuperExpression*)base;
    if (recursive && expr->name != NULL) {
        expr->name->free_fn((DaiAstBase*)expr->name, true);
    }
    dai_free(expr);
}

static char*
DaiAstSuperExpression_literal(DaiAstExpression* base) {
    assert(base->type == DaiAstType_SuperExpression);
    DaiAstSuperExpression* expr = (DaiAstSuperExpression*)base;
    DaiStringBuffer* sb         = DaiStringBuffer_New();
    DaiStringBuffer_write(sb, "(");
    DaiStringBuffer_writen(sb, "super", 5);
    if (expr->name != NULL) {
        DaiStringBuffer_writen(sb, ".", 1);
        char* s = expr->name->literal_fn((DaiAstExpression*)expr->name);
        DaiStringBuffer_write(sb, s);
        dai_free(s);
    }
    DaiStringBuffer_write(sb, ")");
    return DaiStringBuffer_getAndFree(sb, NULL);
}

DaiAstSuperExpression*
DaiAstSuperExpression_New(void) {
    DaiAstSuperExpression* expr = (DaiAstSuperExpression*)dai_malloc(sizeof(DaiAstSuperExpression));
    expr->type                  = DaiAstType_SuperExpression;
    expr->name                  = NULL;
    {
        expr->string_fn  = DaiAstSuperExpression_string;
        expr->free_fn    = DaiAstSuperExpression_free;
        expr->literal_fn = DaiAstSuperExpression_literal;
    }
    {
        expr->start_line   = 0;
        expr->start_column = 0;
        expr->end_line     = 0;
        expr->end_column   = 0;
    }
    return expr;
}
