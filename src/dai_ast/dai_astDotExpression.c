#include <assert.h>

#include "dai_ast/dai_astDotExpression.h"
#include "dai_ast/dai_astcommon.h"
#include "dai_malloc.h"
#include "dai_stringbuffer.h"

static char*
DaiAstDotExpression_string(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_DotExpression);
    DaiAstDotExpression* expr = (DaiAstDotExpression*)base;
    DaiStringBuffer* sb       = DaiStringBuffer_New();
    DaiStringBuffer_write(sb, "{\n");
    DaiStringBuffer_write(sb, indent);
    // DaiStringBuffer_write(sb, "type: DaiAstType_DotExpression,\n");
    DaiStringBuffer_write(sb, KEY_COLOR("type") ": " TYPE_COLOR("DaiAstType_DotExpression") ",\n");
    DaiStringBuffer_write(sb, indent);
    DaiStringBuffer_write(sb, "left: ");
    if (recursive) {
        char* s = expr->left->string_fn((DaiAstBase*)expr->left, true);
        DaiStringBuffer_writeWithLinePrefix(sb, s, indent);
        dai_free(s);
    } else {
        DaiStringBuffer_writePointer(sb, expr->left);
    }
    DaiStringBuffer_write(sb, ",\n");

    DaiStringBuffer_write(sb, indent);
    DaiStringBuffer_write(sb, "name: ");
    {
        char* s = expr->name->string_fn((DaiAstBase*)expr->name, true);
        DaiStringBuffer_writeWithLinePrefix(sb, s, indent);
        dai_free(s);
    }
    DaiStringBuffer_write(sb, ",\n");
    DaiStringBuffer_write(sb, "}");
    return DaiStringBuffer_getAndFree(sb, NULL);
}

static void
DaiAstDotExpression_free(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_DotExpression);
    DaiAstDotExpression* expr = (DaiAstDotExpression*)base;
    if (recursive) {
        if (expr->left != NULL) {
            expr->left->free_fn((DaiAstBase*)expr->left, true);
        }
        if (expr->name != NULL) {
            expr->name->free_fn((DaiAstBase*)expr->name, true);
        }
    }
    dai_free(expr);
}

static char*
DaiAstDotExpression_literal(DaiAstExpression* base) {
    assert(base->type == DaiAstType_DotExpression);
    DaiAstDotExpression* expr = (DaiAstDotExpression*)base;
    DaiStringBuffer* sb       = DaiStringBuffer_New();
    DaiStringBuffer_write(sb, "(");
    {
        char* s = expr->left->literal_fn((DaiAstExpression*)expr->left);
        DaiStringBuffer_write(sb, s);
        dai_free(s);
    }
    DaiStringBuffer_writen(sb, ".", 1);
    {
        char* s = expr->name->literal_fn((DaiAstExpression*)expr->name);
        DaiStringBuffer_write(sb, s);
        dai_free(s);
    }
    DaiStringBuffer_write(sb, ")");
    return DaiStringBuffer_getAndFree(sb, NULL);
}

DaiAstDotExpression*
DaiAstDotExpression_New(DaiAstExpression* left) {
    DaiAstDotExpression* expr = (DaiAstDotExpression*)dai_malloc(sizeof(DaiAstDotExpression));
    expr->type                = DaiAstType_DotExpression;
    {
        expr->free_fn    = DaiAstDotExpression_free;
        expr->string_fn  = DaiAstDotExpression_string;
        expr->literal_fn = DaiAstDotExpression_literal;
    }
    expr->left = left;
    expr->name = NULL;
    {
        expr->start_line   = 0;
        expr->start_column = 0;
        expr->end_line     = 0;
        expr->end_column   = 0;
    }
    return expr;
}
