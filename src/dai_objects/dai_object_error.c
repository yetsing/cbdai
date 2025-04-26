#include "dai_objects/dai_object_error.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dai_common.h"
#include "dai_memory.h"
#include "dai_objects/dai_object_base.h"
#include "dai_windows.h"   // IWYU pragma: keep

const char*
DaiErrorKind_string(DaiErrorKind kind) {
    switch (kind) {
        case DaiError_syntax: return "SyntaxError";
        case DaiError_compile: return "CompileError";
        case DaiError_runtime: return "Error";
        default: unreachable();
    }
}

static char*
DaiObjError_String(DaiValue value, __attribute__((unused)) DaiPtrArray* visited) {
    int size  = strlen(AS_ERROR(value)->message) + 16;
    char* buf = ALLOCATE(char, size);
    snprintf(buf, size, "Error: %s", AS_ERROR(value)->message);
    return buf;
}

static struct DaiObjOperation error_operation = {
    .get_property_func  = NULL,
    .set_property_func  = NULL,
    .subscript_get_func = NULL,
    .subscript_set_func = NULL,
    .string_func        = DaiObjError_String,
    .equal_func         = dai_default_equal,
    .hash_func          = dai_default_hash,
    .iter_init_func     = NULL,
    .iter_next_func     = NULL,
    .get_method_func    = NULL,
};

DaiObjError*
DaiObjError_Newf(DaiVM* vm, const char* format, ...) {
    DaiObjError* error   = ALLOCATE_OBJ(vm, DaiObjError, DaiObjType_error);
    error->obj.operation = &error_operation;
    va_list args;
    va_start(args, format);
    vsnprintf(error->message, sizeof(error->message), format, args);
    va_end(args);
    error->kind = DaiError_runtime;
    return error;
}

DaiObjError*
DaiObjError_From(DaiVM* vm, DaiError* err) {
    DaiObjError* error   = ALLOCATE_OBJ(vm, DaiObjError, DaiObjType_error);
    error->obj.operation = &error_operation;
    strlcpy(error->message, err->msg, sizeof(error->message));
    error->pos  = err->pos;
    error->kind = DaiError_syntax;
    if (strcmp(err->name, "CompileError") == 0) {
        error->kind = DaiError_compile;
    }
    return error;
}
