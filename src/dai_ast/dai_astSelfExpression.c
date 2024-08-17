#include <assert.h>

#include "dai_ast/dai_astSelfExpression.h"
#include "dai_ast/dai_astcommon.h"
#include "dai_malloc.h"
#include "dai_stringbuffer.h"

static char*
DaiAstSelfExpression_string(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_SelfExpression);
    DaiAstSelfExpression* expr = (DaiAstSelfExpression*)base;
    DaiStringBuffer*      sb   = DaiStringBuffer_New();
    DaiStringBuffer_write(sb, "{\n");
    DaiStringBuffer_write(sb, indent);
    // DaiStringBuffer_write(sb, "type: DaiAstType_SelfExpression,\n");
    DaiStringBuffer_write(sb, KEY_COLOR("type") ": " TYPE_COLOR("DaiAstType_SelfExpression") ",\n");

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
DaiAstSelfExpression_free(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_SelfExpression);
    DaiAstSelfExpression* expr = (DaiAstSelfExpression*)base;
    if (recursive && expr->name != NULL) {
        expr->name->free_fn((DaiAstBase*)expr->name, true);
    }
    dai_free(expr);
}

static char*
DaiAstSelfExpression_literal(DaiAstExpression* base) {
    assert(base->type == DaiAstType_SelfExpression);
    DaiAstSelfExpression* expr = (DaiAstSelfExpression*)base;
    DaiStringBuffer*      sb   = DaiStringBuffer_New();
    DaiStringBuffer_write(sb, "(");
    DaiStringBuffer_writen(sb, "self", 4);
    if (expr->name != NULL) {
        DaiStringBuffer_writen(sb, ".", 1);
        char* s = expr->name->literal_fn((DaiAstExpression*)expr->name);
        DaiStringBuffer_write(sb, s);
        dai_free(s);
    }
    DaiStringBuffer_write(sb, ")");
    return DaiStringBuffer_getAndFree(sb, NULL);
}

DaiAstSelfExpression*
DaiAstSelfExpression_New(void) {
    DaiAstSelfExpression* expr = (DaiAstSelfExpression*)dai_malloc(sizeof(DaiAstSelfExpression));
    expr->type                 = DaiAstType_SelfExpression;
    expr->name                 = NULL;
    {
        expr->string_fn  = DaiAstSelfExpression_string;
        expr->free_fn    = DaiAstSelfExpression_free;
        expr->literal_fn = DaiAstSelfExpression_literal;
    }
    {
        expr->start_line   = 0;
        expr->start_column = 0;
        expr->end_line     = 0;
        expr->end_column   = 0;
    }
    return expr;
}
