#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "dai_ast/dai_astcommon.h"
#include "dai_ast/dai_aststringliteral.h"
#include "dai_malloc.h"
#include "dai_stringbuffer.h"

char*
DaiAstStringLiteral_string(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_StringLiteral);
    DaiAstStringLiteral* str = (DaiAstStringLiteral*)base;
    DaiStringBuffer* sb      = DaiStringBuffer_New();
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
DaiAstStringLiteral_free(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_StringLiteral);
    DaiAstStringLiteral* str = (DaiAstStringLiteral*)base;
    dai_free(str->value);
    dai_free(str);
}

char*
DaiAstStringLiteral_literal(DaiAstExpression* expr) {
    assert(expr->type == DaiAstType_StringLiteral);
    DaiAstStringLiteral* str = (DaiAstStringLiteral*)expr;
    return strdup(str->value);
}

char*
handle_escape(const char* s, size_t length) {
    char* ret       = dai_malloc(length + 1);
    char* result    = ret;
    const char* end = s + length;
    while (s < end) {
        if (*s == '\\') {
            s++;
            if (s < end) {
                switch (*s) {
                    case '\\': *result++ = '\\'; break;
                    case 'n': *result++ = '\n'; break;
                    case 't': *result++ = '\t'; break;
                    case 'r': *result++ = '\r'; break;
                    case '"': *result++ = '"'; break;
                    case '\'': *result++ = '\''; break;
                    case 'x': {
                        // \xXX 十六进制转移
                        char seq[3] = {0, 0, 0};
                        s++;
                        seq[0] = *s;
                        s++;
                        seq[1]        = *s;
                        uint8_t value = (uint8_t)strtol(seq, NULL, 16);
                        if (value <= 0x7F) {
                            // 1-byte UTF-8
                            *result++ = value;
                        } else {
                            // 2-byte UTF-8
                            *result++ = 0xC0 | (value >> 6);
                            *result++ = 0x80 | (value & 0x3F);
                        }
                        break;
                    }
                    default: *result++ = *s; break;
                }
            }
        } else {
            *result++ = *s;
        }
        s++;
    }
    *result = '\0';
    return ret;
}

DaiAstStringLiteral*
DaiAstStringLiteral_New(DaiToken* token) {
    DaiAstStringLiteral* str = dai_malloc(sizeof(DaiAstStringLiteral));
    {
        str->type       = DaiAstType_StringLiteral;
        str->string_fn  = DaiAstStringLiteral_string;
        str->free_fn    = DaiAstStringLiteral_free;
        str->literal_fn = DaiAstStringLiteral_literal;
    }
    str->value        = handle_escape(token->s, token->length);
    str->start_line   = token->start_line;
    str->start_column = token->start_column;
    str->end_line     = token->end_line;
    str->end_column   = token->end_column;
    return str;
}