//
// Created by  on 2024/6/5.
//
#include <assert.h>
#include <string.h>

#include "dai_ast/dai_astcommon.h"
#include "dai_ast/dai_astintegerliteral.h"
#include "dai_malloc.h"
#include "dai_stringbuffer.h"

static char*
DaiAstIntegerLiteral_string(DaiAstBase* base, __attribute__((unused)) bool recursive) {
    assert(base->type == DaiAstType_IntegerLiteral);
    DaiAstIntegerLiteral* num = (DaiAstIntegerLiteral*)base;
    DaiStringBuffer* sb       = DaiStringBuffer_New();
    DaiStringBuffer_write(sb, "{\n");
    DaiStringBuffer_write(sb, indent);
    // DaiStringBuffer_write(sb, "type: DaiAstType_IntegerLiteral,\n");
    DaiStringBuffer_write(sb, KEY_COLOR("type") ": " TYPE_COLOR("DaiAstType_IntegerLiteral") ",\n");
    DaiStringBuffer_write(sb, indent);
    DaiStringBuffer_write(sb, "value: ");
    DaiStringBuffer_writeInt64(sb, num->value);
    DaiStringBuffer_write(sb, ",\n");
    DaiStringBuffer_write(sb, "}");
    return DaiStringBuffer_getAndFree(sb, NULL);
}

static void
DaiAstIntegerLiteral_free(DaiAstBase* base, __attribute__((unused)) bool recursive) {
    assert(base->type == DaiAstType_IntegerLiteral);
    DaiAstIntegerLiteral* num = (DaiAstIntegerLiteral*)base;
    dai_free(num->literal);
    dai_free(num);
}

static char*
DaiAstIntegerLiteral_literal(DaiAstExpression* expr) {
    assert(expr->type == DaiAstType_IntegerLiteral);
    DaiAstIntegerLiteral* num = (DaiAstIntegerLiteral*)expr;
    return strdup(num->literal);
}

DaiAstIntegerLiteral*
DaiAstIntegerLiteral_New(DaiToken* token) {
    DaiAstIntegerLiteral* num = dai_malloc(sizeof(DaiAstIntegerLiteral));
    {
        num->type       = DaiAstType_IntegerLiteral;
        num->string_fn  = DaiAstIntegerLiteral_string;
        num->free_fn    = DaiAstIntegerLiteral_free;
        num->literal_fn = DaiAstIntegerLiteral_literal;
    }
    num->value        = -1;
    num->literal      = strndup(token->s, token->length);
    num->start_line   = token->start_line;
    num->start_column = token->start_column;
    num->end_line     = token->end_line;
    num->end_column   = token->end_column;
    return num;
}
