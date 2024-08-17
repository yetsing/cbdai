#include "assert.h"

#include "dai_ast/dai_astcommon.h"
#include "dai_astclassstatement.h"
#include "dai_malloc.h"
#include "dai_stringbuffer.h"

static char*
DaiAstClassStatement_string(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_ClassStatement);
    DaiAstClassStatement* klass = (DaiAstClassStatement*)base;
    DaiStringBuffer*      sb    = DaiStringBuffer_New();
    DaiStringBuffer_write(sb, "{\n");
    DaiStringBuffer_write(sb, indent);
    DaiStringBuffer_write(sb, KEY_COLOR("type") ": " TYPE_COLOR("DaiAstType_ClassStatement") ",\n");
    DaiStringBuffer_write(sb, indent);
    DaiStringBuffer_write(sb, KEY_COLOR("name") ": ");
    DaiStringBuffer_write(sb, klass->name);
    DaiStringBuffer_write(sb, ",\n");
    if (klass->parent != NULL) {
        DaiStringBuffer_write(sb, indent);
        DaiStringBuffer_write(sb, KEY_COLOR("parent") ": ");
        DaiStringBuffer_write(sb, klass->parent->value);
        DaiStringBuffer_write(sb, ",\n");
    }
    DaiStringBuffer_write(sb, indent);
    DaiStringBuffer_write(sb, KEY_COLOR("body") ": ");
    if (recursive && klass->body != NULL) {
        char* s = klass->body->string_fn((DaiAstBase*)klass->body, recursive);
        DaiStringBuffer_writeWithLinePrefix(sb, s, indent);
        dai_free(s);
        DaiStringBuffer_write(sb, ",\n");
    } else {
        DaiStringBuffer_write(sb, "...,\n");
    }
    DaiStringBuffer_write(sb, "}");
    return DaiStringBuffer_getAndFree(sb, NULL);
}

void
DaiAstClassStatement_free(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_ClassStatement);
    DaiAstClassStatement* klass = (DaiAstClassStatement*)base;
    if (recursive) {
        if (klass->body != NULL) {
            klass->body->free_fn((DaiAstBase*)klass->body, recursive);
        }
        if (klass->parent != NULL) {
            klass->parent->free_fn((DaiAstBase*)klass->parent, recursive);
        }
    }
    dai_free(klass->name);
    dai_free(klass);
}

DaiAstClassStatement*
DaiAstClassStatement_New(DaiToken* name) {
    DaiAstClassStatement* klass = dai_malloc(sizeof(DaiAstClassStatement));
    klass->type                 = DaiAstType_ClassStatement;
    {
        klass->string_fn = DaiAstClassStatement_string;
        klass->free_fn   = DaiAstClassStatement_free;
    }
    dai_move(name->literal, klass->name);
    klass->parent = NULL;
    klass->body   = NULL;
    return klass;
}
