//
// Created by  on 2024/6/5.
//
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "dai_ast/dai_astcommon.h"
#include "dai_ast/dai_astprefixexpression.h"
#include "dai_malloc.h"
#include "dai_stringbuffer.h"


static char*
DaiAstPrefixExpression_string(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_PrefixExpression);
    const DaiAstPrefixExpression* expr = (DaiAstPrefixExpression*)base;
    DaiStringBuffer* sb                = DaiStringBuffer_New();
    DaiStringBuffer_write(sb, "{\n");
    DaiStringBuffer_write(sb, indent);
    // DaiStringBuffer_write(sb, "type: DaiAstType_PrefixExpression,\n");
    DaiStringBuffer_write(sb,
                          KEY_COLOR("type") ": " TYPE_COLOR("DaiAstType_PrefixExpression") ",\n");
    DaiStringBuffer_write(sb, indent);
    DaiStringBuffer_write(sb, "operator: ");
    DaiStringBuffer_writen(sb, expr->operator, strlen(expr->operator));
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
DaiAstPrefixExpression_free(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_PrefixExpression);
    DaiAstPrefixExpression* expr = (DaiAstPrefixExpression*)base;
    dai_free(expr->operator);
    if (recursive && expr->right != NULL) {
        expr->right->free_fn((DaiAstBase*)expr->right, recursive);
    }
    dai_free(expr);
}

static char*
DaiAstPrefixExpression_literal(DaiAstExpression* expr) {
    assert(expr->type == DaiAstType_PrefixExpression);
    const DaiAstPrefixExpression* prefix = (DaiAstPrefixExpression*)expr;
    char buf[256];
    char* expr_str = prefix->right->literal_fn(prefix->right);
    snprintf(buf, 256, "(%s %s)", prefix->operator, expr_str);
    dai_free(expr_str);
    return strdup(buf);
}

DaiAstPrefixExpression*
DaiAstPrefixExpression_New(const DaiToken* operator, DaiAstExpression * right) {
    DaiAstPrefixExpression* expr = dai_malloc(sizeof(DaiAstPrefixExpression));
    {
        expr->type       = DaiAstType_PrefixExpression;
        expr->string_fn  = DaiAstPrefixExpression_string;
        expr->free_fn    = DaiAstPrefixExpression_free;
        expr->literal_fn = DaiAstPrefixExpression_literal;
    }
    const DaiToken* token = operator;
    expr->operator= strdup(token->literal);
    expr->right        = right;
    expr->start_line   = token->start_line;
    expr->start_column = token->start_column;
    return expr;
}
