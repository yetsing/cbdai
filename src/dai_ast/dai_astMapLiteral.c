#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "dai_ast/dai_astMapLiteral.h"
#include "dai_ast/dai_astcommon.h"
#include "dai_ast/dai_asttype.h"
#include "dai_malloc.h"
#include "dai_stringbuffer.h"

static char*
DaiAstMapLiteral_string(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_MapLiteral);
    DaiAstMapLiteral* map = (DaiAstMapLiteral*)base;
    DaiStringBuffer* sb   = DaiStringBuffer_New();
    DaiStringBuffer_write(sb, "{\n");
    DaiStringBuffer_write(sb, indent);
    DaiStringBuffer_write(sb, KEY_COLOR("type") ": " TYPE_COLOR("DaiAstType_MapLiteral") ",\n");
    DaiStringBuffer_write(sb, indent);
    DaiStringBuffer_write(sb, "length: ");
    DaiStringBuffer_writeInt64(sb, map->length);
    DaiStringBuffer_write(sb, ",\n");
    if (recursive) {
        DaiStringBuffer_write(sb, indent);
        DaiStringBuffer_write(sb, "pairs: [\n");
        char* s;
        for (size_t i = 0; i < map->length; i++) {
            DaiAstMapLiteralPair pair = map->pairs[i];
            DaiStringBuffer_write(sb, indent);
            DaiStringBuffer_write(sb, "  {\n");
            DaiStringBuffer_write(sb, indent);
            DaiStringBuffer_write(sb, "    key: ");
            s = pair.key->string_fn((DaiAstBase*)pair.key, recursive);
            DaiStringBuffer_writeWithLinePrefix(sb, s, doubleindent);
            dai_free(s);
            DaiStringBuffer_write(sb, ",\n");
            DaiStringBuffer_write(sb, indent);
            DaiStringBuffer_write(sb, "    value: ");
            s = pair.value->string_fn((DaiAstBase*)pair.value, recursive);
            DaiStringBuffer_writeWithLinePrefix(sb, s, doubleindent);
            dai_free(s);
            DaiStringBuffer_write(sb, "\n");
            DaiStringBuffer_write(sb, indent);
            DaiStringBuffer_write(sb, "  }");
            DaiStringBuffer_write(sb, ",");
            DaiStringBuffer_write(sb, "\n");
        }
        DaiStringBuffer_write(sb, indent);
        DaiStringBuffer_write(sb, "],\n");
    } else {
        DaiStringBuffer_write(sb, indent);
        DaiStringBuffer_write(sb, "pairs: [...],\n");
    }
    DaiStringBuffer_write(sb, "}");
    return DaiStringBuffer_getAndFree(sb, NULL);
}

static void
DaiAstMapLiteral_free(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_MapLiteral);
    DaiAstMapLiteral* map = (DaiAstMapLiteral*)base;
    if (recursive && map->pairs) {
        for (size_t i = 0; i < map->length; i++) {
            DaiAstMapLiteralPair pair = map->pairs[i];
            pair.key->free_fn((DaiAstBase*)pair.key, recursive);
            pair.value->free_fn((DaiAstBase*)pair.value, recursive);
        }
    }
    dai_free(map->pairs);
    dai_free(map);
}

static char*
DaiAstMapLiteral_literal(DaiAstExpression* base) {
    assert(base->type == DaiAstType_MapLiteral);
    const DaiAstMapLiteral* map = (const DaiAstMapLiteral*)base;
    DaiStringBuffer* sb         = DaiStringBuffer_New();
    DaiStringBuffer_write(sb, "{");
    for (size_t i = 0; i < map->length; i++) {
        DaiAstMapLiteralPair pair = map->pairs[i];
        DaiAstExpression* k       = pair.key;
        DaiAstExpression* v       = pair.value;
        char* ks                  = k->literal_fn(k);
        char* vs                  = v->literal_fn(v);
        DaiStringBuffer_write(sb, ks);
        DaiStringBuffer_write(sb, ": ");
        DaiStringBuffer_write(sb, vs);
        dai_free(ks);
        dai_free(vs);
        DaiStringBuffer_write(sb, ", ");
    }
    DaiStringBuffer_write(sb, "}");
    return DaiStringBuffer_getAndFree(sb, NULL);
}

DaiAstMapLiteral*
DaiAstMapLiteral_New(void) {
    DaiAstMapLiteral* map = dai_malloc(sizeof(DaiAstMapLiteral));
    {
        map->type       = DaiAstType_MapLiteral;
        map->string_fn  = DaiAstMapLiteral_string;
        map->free_fn    = DaiAstMapLiteral_free;
        map->literal_fn = DaiAstMapLiteral_literal;
    }
    map->length = 0;
    map->pairs  = NULL;
    return map;
}