#include <assert.h>
#include <string.h>

#include "dai_ast/dai_astcommon.h"
#include "dai_ast/dai_astinfixexpression.h"
#include "dai_malloc.h"
#include "dai_stringbuffer.h"

static char*
DaiAstInfixExpression_string(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_InfixExpression);
    DaiAstInfixExpression* expr = (DaiAstInfixExpression*)base;
    DaiStringBuffer* sb         = DaiStringBuffer_New();
    DaiStringBuffer_write(sb, "{\n");
    DaiStringBuffer_write(sb, indent);
    // DaiStringBuffer_write(sb, "type: DaiAstType_InfixExpression,\n");
    DaiStringBuffer_write(sb,
                          KEY_COLOR("type") ": " TYPE_COLOR("DaiAstType_InfixExpression") ",\n");

    DaiStringBuffer_write(sb, indent);
    DaiStringBuffer_write(sb, "operator: ");
    DaiStringBuffer_writen(sb, expr->operator, strlen(expr->operator));
    DaiStringBuffer_write(sb, ",\n");

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
    DaiStringBuffer_write(sb, "right: ");
    if (recursive) {
        char* s = expr->right->string_fn((DaiAstBase*)expr->right, true);
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
DaiAstInfixExpression_free(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_InfixExpression);
    DaiAstInfixExpression* expr = (DaiAstInfixExpression*)base;
    if (recursive) {
        expr->left->free_fn((DaiAstBase*)expr->left, true);
        if (expr->right != NULL) {
            expr->right->free_fn((DaiAstBase*)expr->right, true);
        }
    }
    dai_free(expr->operator);
    dai_free(expr);
}

static char*
DaiAstInfixExpression_literal(DaiAstExpression* base) {
    assert(base->type == DaiAstType_InfixExpression);
    DaiAstInfixExpression* expr = (DaiAstInfixExpression*)base;
    DaiStringBuffer* sb         = DaiStringBuffer_New();
    DaiStringBuffer_write(sb, "(");
    {
        char* s = expr->left->literal_fn(expr->left);
        DaiStringBuffer_write(sb, s);
        dai_free(s);
    }
    DaiStringBuffer_writen(sb, " ", 1);
    DaiStringBuffer_writen(sb, expr->operator, strlen(expr->operator));
    DaiStringBuffer_writen(sb, " ", 1);
    {
        char* s = expr->right->literal_fn(expr->right);
        DaiStringBuffer_write(sb, s);
        dai_free(s);
    }
    DaiStringBuffer_write(sb, ")");
    return DaiStringBuffer_getAndFree(sb, NULL);
}

DaiAstInfixExpression*
DaiAstInfixExpression_New(const char* operator, DaiAstExpression * left, DaiAstExpression* right) {
    DaiAstInfixExpression* expr = (DaiAstInfixExpression*)dai_malloc(sizeof(DaiAstInfixExpression));
    expr->type                  = DaiAstType_InfixExpression;
    expr->string_fn             = DaiAstInfixExpression_string;
    expr->free_fn               = DaiAstInfixExpression_free;
    expr->literal_fn            = DaiAstInfixExpression_literal;
    expr->operator= strdup(operator);
    expr->left         = left;
    expr->start_line   = left->start_line;
    expr->start_column = left->start_column;
    expr->right        = right;
    return expr;
}