#include "dai_objects/dai_object_function.h"

#include <stdio.h>

#include "dai_memory.h"
#include "dai_objects/dai_object_string.h"


// #region function

static char*
DaiObjFunction_String(DaiValue value, __attribute__((unused)) DaiPtrArray* visited) {
    DaiObjFunction* function = AS_FUNCTION(value);
    int size                 = function->name->length + 16;
    char* buf                = ALLOCATE(char, size);
    snprintf(buf, size, "<fn %s>", function->name->chars);
    return buf;
}

static struct DaiObjOperation function_operation = {
    .get_property_func  = NULL,
    .set_property_func  = NULL,
    .subscript_get_func = NULL,
    .subscript_set_func = NULL,
    .string_func        = DaiObjFunction_String,
    .equal_func         = dai_default_equal,
    .hash_func          = dai_default_hash,
    .iter_init_func     = NULL,
    .iter_next_func     = NULL,
    .get_method_func    = NULL,
};

DaiObjFunction*
DaiObjFunction_New(DaiVM* vm, DaiObjModule* module, const char* name, const char* filename) {
    DaiObjFunction* function = ALLOCATE_OBJ(vm, DaiObjFunction, DaiObjType_function);
    function->obj.operation  = &function_operation;
    function->arity          = 0;
    function->name           = dai_copy_string_intern(vm, name, strlen(name));
    DaiChunk_init(&function->chunk, filename);
    function->superclass      = NULL;
    function->defaults        = NULL;
    function->default_count   = 0;
    function->module          = module;
    function->max_local_count = 0;
    return function;
}

const char*
DaiObjFunction_name(DaiObjFunction* function) {
    return function->name->chars;
}

// #endregion

// #region closure

static char*
DaiObjClosure_String(DaiValue value, __attribute__((unused)) DaiPtrArray* visited) {
    DaiObjClosure* closure = AS_CLOSURE(value);
    int size               = closure->function->name->length + 16;
    char* buf              = ALLOCATE(char, size);
    snprintf(buf, size, "<closure %s>", closure->function->name->chars);
    return buf;
}

static struct DaiObjOperation closure_operation = {
    .get_property_func  = NULL,
    .set_property_func  = NULL,
    .subscript_get_func = NULL,
    .subscript_set_func = NULL,
    .string_func        = DaiObjClosure_String,
    .equal_func         = dai_default_equal,
    .hash_func          = dai_default_hash,
    .iter_init_func     = NULL,
    .iter_next_func     = NULL,
    .get_method_func    = NULL,
};

DaiObjClosure*
DaiObjClosure_New(DaiVM* vm, DaiObjFunction* function) {
    DaiObjClosure* closure = ALLOCATE_OBJ(vm, DaiObjClosure, DaiObjType_closure);
    closure->obj.operation = &closure_operation;
    closure->function      = function;
    closure->frees         = NULL;
    closure->free_count    = 0;
    return closure;
}

__attribute__((unused)) const char*
DaiObjClosure_name(DaiObjClosure* closure) {
    return closure->function->name->chars;
}

// #endregion


// #region builtin function operation

static char*
DaiObjBuiltinFunction_String(DaiValue value, __attribute__((unused)) DaiPtrArray* visited) {
    DaiObjBuiltinFunction* builtin = AS_BUILTINFN(value);
    size_t size                    = strlen(builtin->name) + 16;
    char* buf                      = ALLOCATE(char, size);
    snprintf(buf, size, "<builtin-fn %s>", builtin->name);
    return buf;
}

struct DaiObjOperation builtin_function_operation = {
    .get_property_func  = NULL,
    .set_property_func  = NULL,
    .subscript_get_func = NULL,
    .subscript_set_func = NULL,
    .string_func        = DaiObjBuiltinFunction_String,
    .equal_func         = dai_default_equal,
    .hash_func          = dai_default_hash,
    .iter_init_func     = NULL,
    .iter_next_func     = NULL,
    .get_method_func    = NULL,
};

// #endregion


// #region DaiObjCFunction
static char*
DaiObjCFunction_String(DaiValue value, __attribute__((unused)) DaiPtrArray* visited) {
    DaiObjCFunction* c_func = AS_CFUNCTION(value);
    size_t size             = strlen(c_func->name) + 16;
    char* buf               = ALLOCATE(char, size);
    snprintf(buf, size, "<c-fn %s>", c_func->name);
    return buf;
}

static struct DaiObjOperation c_function_operation = {
    .get_property_func  = NULL,
    .set_property_func  = NULL,
    .subscript_get_func = NULL,
    .subscript_set_func = NULL,
    .string_func        = DaiObjCFunction_String,
    .equal_func         = dai_default_equal,
    .hash_func          = dai_default_hash,
    .iter_init_func     = NULL,
    .iter_next_func     = NULL,
    .get_method_func    = NULL,
};

DaiObjCFunction*
DaiObjCFunction_New(DaiVM* vm, void* dai, BuiltinFn wrapper, CFunction func, const char* name,
                    int arity) {
    DaiObjCFunction* c_func = ALLOCATE_OBJ(vm, DaiObjCFunction, DaiObjType_cFunction);
    c_func->obj.operation   = &c_function_operation;
    c_func->dai             = dai;
    c_func->wrapper         = wrapper;
    c_func->name            = strdup(name);
    c_func->func            = func;
    c_func->arity           = arity;
    return c_func;
}

// #endregion
