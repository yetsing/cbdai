#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "dai_ast/dai_astArrayLiteral.h"
#include "dai_ast/dai_astcommon.h"
#include "dai_malloc.h"
#include "dai_stringbuffer.h"

char*
DaiAstArrayLiteral_string(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_ArrayLiteral);
    DaiAstArrayLiteral* array = (DaiAstArrayLiteral*)base;
    DaiStringBuffer* sb       = DaiStringBuffer_New();
    DaiStringBuffer_write(sb, "{\n");
    DaiStringBuffer_write(sb, indent);
    // DaiStringBuffer_write(sb, "type: DaiAstType_ArrayLiteral,\n");
    DaiStringBuffer_write(sb, KEY_COLOR("type") ": " TYPE_COLOR("DaiAstType_ArrayLiteral") ",\n");
    DaiStringBuffer_write(sb, indent);
    DaiStringBuffer_write(sb, "length: ");
    DaiStringBuffer_writeInt64(sb, array->length);
    DaiStringBuffer_write(sb, ",\n");
    if (recursive) {
        DaiStringBuffer_write(sb, indent);
        DaiStringBuffer_write(sb, "elements: [\n");
        DaiAstExpression** elements = array->elements;
        for (size_t i = 0; i < array->length; i++) {
            char* s = elements[i]->string_fn((DaiAstBase*)elements[i], true);
            DaiStringBuffer_writeWithLinePrefix(sb, s, doubleindent);
            DaiStringBuffer_write(sb, ",\n");
            dai_free(s);
        }
        DaiStringBuffer_write(sb, indent);
        DaiStringBuffer_write(sb, "],\n");
    } else {
        DaiStringBuffer_write(sb, indent);
        DaiStringBuffer_write(sb, "elements: [...],\n");
    }
    DaiStringBuffer_write(sb, "}");
    return DaiStringBuffer_getAndFree(sb, NULL);
};

void
DaiAstArrayLiteral_free(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_ArrayLiteral);
    DaiAstArrayLiteral* array = (DaiAstArrayLiteral*)base;
    if (recursive) {
        for (size_t i = 0; i < array->length; i++) {
            array->elements[i]->free_fn((DaiAstBase*)array->elements[i], recursive);
        }
    };
    dai_free(array->elements);
    dai_free(array);
}


char*
DaiAstArrayLiteral_literal(DaiAstExpression* base) {
    assert(base->type == DaiAstType_ArrayLiteral);
    const DaiAstArrayLiteral* array = (const DaiAstArrayLiteral*)base;
    DaiStringBuffer* sb             = DaiStringBuffer_New();
    DaiStringBuffer_write(sb, "[");
    for (size_t i = 0; i < array->length; i++) {
        DaiAstExpression* e = array->elements[i];
        char* s             = e->literal_fn(e);
        DaiStringBuffer_write(sb, s);
        dai_free(s);
        DaiStringBuffer_write(sb, ", ");
    }
    DaiStringBuffer_write(sb, "]");
    return DaiStringBuffer_getAndFree(sb, NULL);
}

DaiAstArrayLiteral*
DaiAstArrayLiteral_New(void) {
    DaiAstArrayLiteral* array = dai_malloc(sizeof(DaiAstArrayLiteral));
    DAI_AST_EXPRESSION_INIT(array);
    {
        array->type       = DaiAstType_ArrayLiteral;
        array->string_fn  = DaiAstArrayLiteral_string;
        array->free_fn    = DaiAstArrayLiteral_free;
        array->literal_fn = DaiAstArrayLiteral_literal;
    }
    array->length   = 0;
    array->elements = NULL;
    return array;
}
