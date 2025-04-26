#include <assert.h>
#include <string.h>

#include "dai_ast/dai_astFloatLiteral.h"
#include "dai_ast/dai_astcommon.h"
#include "dai_malloc.h"
#include "dai_stringbuffer.h"

static char*
DaiAstFloatLiteral_string(DaiAstBase* base, __attribute__((unused)) bool recursive) {
    assert(base->type == DaiAstType_FloatLiteral);
    DaiAstFloatLiteral* num = (DaiAstFloatLiteral*)base;
    DaiStringBuffer* sb     = DaiStringBuffer_New();
    DaiStringBuffer_write(sb, "{\n");
    DaiStringBuffer_write(sb, indent);
    // DaiStringBuffer_write(sb, "type: DaiAstType_FloatLiteral,\n");
    DaiStringBuffer_write(sb, KEY_COLOR("type") ": " TYPE_COLOR("DaiAstType_FloatLiteral") ",\n");
    DaiStringBuffer_write(sb, indent);
    DaiStringBuffer_write(sb, "value: ");
    DaiStringBuffer_writeDouble(sb, num->value);
    DaiStringBuffer_write(sb, ",\n");
    DaiStringBuffer_write(sb, "}");
    return DaiStringBuffer_getAndFree(sb, NULL);
}

static void
DaiAstFloatLiteral_free(DaiAstBase* base, __attribute__((unused)) bool recursive) {
    assert(base->type == DaiAstType_FloatLiteral);
    DaiAstFloatLiteral* num = (DaiAstFloatLiteral*)base;
    dai_free(num->literal);
    dai_free(num);
}

static char*
DaiAstFloatLiteral_literal(DaiAstExpression* expr) {
    assert(expr->type == DaiAstType_FloatLiteral);
    DaiAstFloatLiteral* num = (DaiAstFloatLiteral*)expr;
    return strdup(num->literal);
}

DaiAstFloatLiteral*
DaiAstFloatLiteral_New(DaiToken* token) {
    DaiAstFloatLiteral* num = dai_malloc(sizeof(DaiAstFloatLiteral));
    {
        num->type       = DaiAstType_FloatLiteral;
        num->string_fn  = DaiAstFloatLiteral_string;
        num->free_fn    = DaiAstFloatLiteral_free;
        num->literal_fn = DaiAstFloatLiteral_literal;
    }
    num->value        = -1;
    num->literal      = strndup(token->s, token->length);
    num->start_line   = token->start_line;
    num->start_column = token->start_column;
    num->end_line     = token->end_line;
    num->end_column   = token->end_column;
    return num;
}
