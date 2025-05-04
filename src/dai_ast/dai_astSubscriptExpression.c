#include <assert.h>
#include <stdio.h>

#include "dai_ast/dai_astSubscriptExpression.h"
#include "dai_ast/dai_astcommon.h"
#include "dai_malloc.h"
#include "dai_stringbuffer.h"


static char*
DaiAstSubscriptExpression_string(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_SubscriptExpression);
    const DaiAstSubscriptExpression* expr = (DaiAstSubscriptExpression*)base;
    DaiStringBuffer* sb                   = DaiStringBuffer_New();
    DaiStringBuffer_write(sb, "{\n");
    DaiStringBuffer_write(sb, indent);
    // DaiStringBuffer_write(sb, "type: DaiAstType_SubscriptExpression,\n");
    DaiStringBuffer_write(
        sb, KEY_COLOR("type") ": " TYPE_COLOR("DaiAstType_SubscriptExpression") ",\n");
    DaiStringBuffer_write(sb, indent);
    DaiStringBuffer_write(sb, "left: ");
    if (recursive) {
        char* s = expr->left->string_fn((DaiAstBase*)expr->left, recursive);
        DaiStringBuffer_writeWithLinePrefix(sb, s, indent);
        dai_free(s);
    } else {
        DaiStringBuffer_writePointer(sb, expr->right);
    }
    DaiStringBuffer_write(sb, ",\n");

    DaiStringBuffer_write(sb, indent);
    DaiStringBuffer_write(sb, "right: ");
    if (recursive) {
        char* s = expr->right->string_fn((DaiAstBase*)expr->right, recursive);
        DaiStringBuffer_writeWithLinePrefix(sb, s, indent);
        dai_free(s);
    } else {
        DaiStringBuffer_writePointer(sb, expr->right);
    }
    DaiStringBuffer_write(sb, ",\n");
    DaiStringBuffer_write(sb, "}");
    return DaiStringBuffer_getAndFree(sb, NULL);
}

static void
DaiAstSubscriptExpression_free(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_SubscriptExpression);
    DaiAstSubscriptExpression* expr = (DaiAstSubscriptExpression*)base;
    if (recursive) {
        if (expr->left) {
            expr->left->free_fn((DaiAstBase*)expr->left, recursive);
        }
        if (expr->right) {
            expr->right->free_fn((DaiAstBase*)expr->right, recursive);
        }
    }
    dai_free(expr);
}

static char*
DaiAstSubscriptExpression_literal(DaiAstExpression* expr) {
    assert(expr->type == DaiAstType_SubscriptExpression);
    const DaiAstSubscriptExpression* subscript = (DaiAstSubscriptExpression*)expr;
    DaiStringBuffer* sb                        = DaiStringBuffer_New();
    DaiStringBuffer_write(sb, "(");
    char* s = subscript->left->literal_fn(subscript->left);
    DaiStringBuffer_write(sb, s);
    dai_free(s);
    DaiStringBuffer_write(sb, "[");
    s = subscript->right->literal_fn(subscript->right);
    DaiStringBuffer_write(sb, s);
    dai_free(s);
    DaiStringBuffer_write(sb, "]");
    DaiStringBuffer_write(sb, ")");
    return DaiStringBuffer_getAndFree(sb, NULL);
}

DaiAstSubscriptExpression*
DaiAstSubscriptExpression_New(DaiAstExpression* left) {
    DaiAstSubscriptExpression* expr = dai_malloc(sizeof(DaiAstSubscriptExpression));
    DAI_AST_EXPRESSION_INIT(expr);
    {
        expr->type       = DaiAstType_SubscriptExpression;
        expr->string_fn  = DaiAstSubscriptExpression_string;
        expr->free_fn    = DaiAstSubscriptExpression_free;
        expr->literal_fn = DaiAstSubscriptExpression_literal;
    }
    expr->left         = left;
    expr->start_line   = left->start_line;
    expr->start_column = left->start_column;
    expr->right        = NULL;
    return expr;
}
