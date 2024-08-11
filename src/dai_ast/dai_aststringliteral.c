#include <assert.h>
#include <string.h>
#include <stdint.h>

#include "dai_ast/dai_aststringliteral.h"
#include "dai_ast/dai_astcommon.h"
#include "dai_stringbuffer.h"
#include "dai_malloc.h"

char *
DaiAstStringLiteral_string(DaiAstBase *base, bool recursive) {
    assert(base->type == DaiAstType_StringLiteral);
    DaiAstStringLiteral *str = (DaiAstStringLiteral *) base;
    DaiStringBuffer *sb = DaiStringBuffer_New();
    DaiStringBuffer_write(sb, "{\n");
    DaiStringBuffer_write(sb, indent);
    // DaiStringBuffer_write(sb, "type: DaiAstType_StringLiteral,\n");
  DaiStringBuffer_write(sb, KEY_COLOR("type") ": " TYPE_COLOR("DaiAstType_StringLiteral") ",\n");
    DaiStringBuffer_write(sb, indent);
    DaiStringBuffer_write(sb, "value: ");
    DaiStringBuffer_write(sb, GREEN);
    DaiStringBuffer_writen(sb, str->value, strlen(str->value));
    DaiStringBuffer_write(sb, RESET);
    DaiStringBuffer_write(sb, ",\n");
    DaiStringBuffer_write(sb, "}");
    return DaiStringBuffer_getAndFree(sb, NULL);
}

void
DaiAstStringLiteral_free(DaiAstBase *base, bool recursive) {
    assert(base->type == DaiAstType_StringLiteral);
    DaiAstStringLiteral *str = (DaiAstStringLiteral *) base;
    dai_free(str->value);
    dai_free(str);
}

char *
DaiAstStringLiteral_literal(DaiAstExpression *expr) {
    assert(expr->type == DaiAstType_StringLiteral);
    DaiAstStringLiteral *str = (DaiAstStringLiteral *) expr;
    return strdup(str->value);
}

DaiAstStringLiteral *
DaiAstStringLiteral_New(DaiToken *token) {
    DaiAstStringLiteral *str = dai_malloc(sizeof(DaiAstStringLiteral));
    {
        str->type = DaiAstType_StringLiteral;
        str->string_fn = DaiAstStringLiteral_string;
        str->free_fn = DaiAstStringLiteral_free;
        str->literal_fn = DaiAstStringLiteral_literal;
    }
    dai_move(token->literal, str->value);
    str->start_line = token->start_line;
    str->start_column = token->start_column;
    str->end_line = token->end_line;
    str->end_column = token->end_column;
    return str;
}