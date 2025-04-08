#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cwalk.h"
#include "dai_utils.h"
#include "hashmap.h"
#include "utf8.h"

#include "dai_memory.h"
#include "dai_object.h"
#include "dai_stringbuffer.h"
#include "dai_symboltable.h"
#include "dai_table.h"
#include "dai_value.h"
#include "dai_vm.h"

#define ALLOCATE_OBJ(vm, type, objectType) (type*)allocate_object(vm, sizeof(type), objectType)

static DaiObjString*
dai_find_string_intern(DaiVM* vm, const char* chars, int length);

// #region DaiPropertyOffset
typedef struct {
    DaiObjString* property;   // property name
    int offset;
} DaiPropertyOffset;

static int
DaiPropertyOffset_compare(const void* a, const void* b, void* udata) {
    const DaiPropertyOffset* offset_a = a;
    const DaiPropertyOffset* offset_b = b;
    if (offset_a->property == offset_b->property) {
        return 0;
    }
    return strcmp(offset_a->property->chars, offset_b->property->chars);
}

static uint64_t
DaiPropertyOffset_hash(const void* item, uint64_t seed0, uint64_t seed1) {
    const DaiPropertyOffset* offset = item;
    return offset->property->hash;
}

// #endregion

// #region default operation

static DaiValue
dai_default_get_property(DaiVM* vm, DaiValue receiver, DaiObjString* name) {
    DaiObjError* err = DaiObjError_Newf(
        vm, "'%s' object has not property '%s'", dai_value_ts(receiver), name->chars);
    return OBJ_VAL(err);
}

static DaiValue
dai_default_set_property(DaiVM* vm, DaiValue receiver, DaiObjString* name, DaiValue value) {
    DaiObjError* err = DaiObjError_Newf(
        vm, "'%s' object can not set property '%s'", dai_value_ts(receiver), name->chars);
    return OBJ_VAL(err);
}

static DaiValue
dai_default_subscript_get(DaiVM* vm, DaiValue receiver, DaiValue index) {
    DaiObjError* err =
        DaiObjError_Newf(vm, "'%s' object is not subscriptable", dai_value_ts(receiver));
    return OBJ_VAL(err);
}

static DaiValue
dai_default_subscript_set(DaiVM* vm, DaiValue receiver, DaiValue index, DaiValue value) {
    DaiObjError* err =
        DaiObjError_Newf(vm, "'%s' object is not subscriptable", dai_value_ts(receiver));
    return OBJ_VAL(err);
}

static char*
dai_default_string_func(DaiValue value, __attribute__((unused)) DaiPtrArray* visited) {
    char buf[64];
    int length = snprintf(buf, sizeof(buf), "<obj at %p>", AS_OBJ(value));
    return strndup(buf, length);
}

static int
dai_default_equal(DaiValue a, DaiValue b, __attribute__((unused)) int* limit) {
    return AS_OBJ(a) == AS_OBJ(b);
}

static uint64_t
dai_default_hash(DaiValue value) {
    return (uint64_t)(uintptr_t)AS_OBJ(value);
}

static DaiValue
dai_iter_init_return_self(DaiVM* vm, DaiValue receiver) {
    return receiver;
}

static DaiValue
dai_default_get_method(DaiVM* vm, DaiValue receiver, DaiObjString* name) {
    DaiObjError* err = DaiObjError_Newf(
        vm, "'%s' object has not method '%s'", dai_value_ts(receiver), name->chars);
    return OBJ_VAL(err);
}

static struct DaiOperation default_operation = {
    .get_property_func  = dai_default_get_property,
    .set_property_func  = dai_default_set_property,
    .subscript_get_func = dai_default_subscript_get,
    .subscript_set_func = dai_default_subscript_set,
    .string_func        = dai_default_string_func,
    .equal_func         = dai_default_equal,
    .hash_func          = dai_default_hash,
    .iter_init_func     = NULL,
    .iter_next_func     = NULL,
    .get_method_func    = dai_default_get_method,
};

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

struct DaiOperation builtin_function_operation = {
    .get_property_func  = dai_default_get_property,
    .set_property_func  = dai_default_set_property,
    .subscript_get_func = dai_default_subscript_get,
    .subscript_set_func = dai_default_subscript_set,
    .string_func        = DaiObjBuiltinFunction_String,
    .equal_func         = dai_default_equal,
    .hash_func          = dai_default_hash,
    .iter_init_func     = NULL,
    .iter_next_func     = NULL,
    .get_method_func    = dai_default_get_method,
};

// #endregion

static DaiObj*
allocate_object(DaiVM* vm, size_t size, DaiObjType type) {
    DaiObj* object    = (DaiObj*)vm_reallocate(vm, NULL, 0, size);
    object->type      = type;
    object->is_marked = false;
    object->next      = vm->objects;
    object->operation = &default_operation;
    vm->objects       = object;
#ifdef DEBUG_LOG_GC
    dai_log("%p allocate %zu for %d\n", (void*)object, size, type);

#endif

    return object;
}

// #region module

static bool
DaiObjModule_get_global1(DaiObjModule* module, DaiObjString* name, DaiValue* value) {
    DaiPropertyOffset offset = {
        .property = name,
        .offset   = 0,
    };
    const void* res = hashmap_get(module->global_map, &offset);
    if (res == NULL) {
        return false;
    }
    *value = module->globals[((DaiPropertyOffset*)res)->offset];
    return true;
}

DaiValue
DaiObjModule_get_property(DaiVM* vm, DaiValue receiver, DaiObjString* name) {
    DaiObjModule* module = AS_MODULE(receiver);
    DaiValue value;
    if (DaiObjModule_get_global1(module, name, &value)) {
        return value;
    }
    DaiObjError* err = DaiObjError_Newf(
        vm, "'%s' object has not property '%s'", dai_object_ts(receiver), name->chars);
    return OBJ_VAL(err);
}

char*
DaiObjModule_String(DaiValue value, __attribute__((unused)) DaiPtrArray* visited) {
    DaiObjModule* module = AS_MODULE(value);
    int size             = module->name->length + 16;
    char* buf            = ALLOCATE(char, size);
    snprintf(buf, size, "<module %s>", module->name->chars);
    return buf;
}

static struct DaiOperation module_operation = {
    .get_property_func  = DaiObjModule_get_property,
    .set_property_func  = dai_default_set_property,
    .subscript_get_func = dai_default_subscript_get,
    .subscript_set_func = dai_default_subscript_set,
    .string_func        = DaiObjModule_String,
    .equal_func         = dai_default_equal,
    .hash_func          = dai_default_hash,
    .iter_init_func     = NULL,
    .iter_next_func     = NULL,
    .get_method_func    = DaiObjModule_get_property,
};

// note: name 和 filename 要在堆上分配，同时转移所有权给 module
DaiObjModule*
DaiObjModule_New(DaiVM* vm, const char* name, const char* filename) {
    assert(name != NULL && filename != NULL);
    DaiObjModule* module  = ALLOCATE_OBJ(vm, DaiObjModule, DaiObjType_module);
    module->obj.operation = &module_operation;
    module->name          = dai_take_string_intern(vm, (char*)name, strlen(name));
    module->filename      = dai_take_string_intern(vm, (char*)filename, strlen(filename));
    // filename 在调用 dai_take_string_intern 可能会被释放，所以这里从结果的 module->filename 中取
    DaiChunk_init(&module->chunk, module->filename->chars);
    module->globals = malloc(sizeof(DaiValue) * GLOBAL_MAX);
    if (module->globals == NULL) {
        dai_error("malloc globals(%zu bytes) error\n", GLOBAL_MAX * sizeof(DaiValue));
        abort();
    }
    for (int i = 0; i < GLOBAL_MAX; i++) {
        module->globals[i] = UNDEFINED_VAL;
    }
    // DaiSymbolTable_setOuter(module->globalSymbolTable, vm->builtinSymbolTable);
    module->compiled        = false;
    module->max_local_count = 0;

    module->vm = vm;

    module->predefine_global_count = 0;
    uint64_t seed0, seed1;
    DaiVM_getSeed2(vm, &seed0, &seed1);
    module->global_map = hashmap_new(sizeof(DaiPropertyOffset),
                                     8,
                                     seed0,
                                     seed1,
                                     DaiPropertyOffset_hash,
                                     DaiPropertyOffset_compare,
                                     NULL,
                                     vm);
    if (module->global_map == NULL) {
        dai_error("DaiObjModule_New: Out of memory\n");
        abort();
    }

    // 设置两个内置的全局变量 __name__ 和 __file__
    DaiObjModule_add_global(module, "__name__", OBJ_VAL(module->name));
    DaiObjModule_add_global(module, "__file__", OBJ_VAL(module->filename));
    return module;
}

void
DaiObjModule_Free(DaiVM* vm, DaiObjModule* module) {
    assert(vm == module->vm);
    DaiChunk_reset(&module->chunk);
    hashmap_free(module->global_map);
    free(module->globals);
    VM_FREE(vm, DaiObjModule, module);
}

void
DaiObjModule_beforeCompile(DaiObjModule* module, DaiSymbolTable* symbol_table) {
    assert(!(module->compiled));
    // 按顺序排好
    size_t count = hashmap_count(module->global_map);
    assert(count == module->predefine_global_count);
    DaiPropertyOffset* offsets = malloc(sizeof(DaiPropertyOffset) * count);
    if (offsets == NULL) {
        dai_error("malloc offsets(%zu bytes) error\n", count * sizeof(DaiPropertyOffset));
        abort();
    }
    void* item;
    size_t iter = 0;
    while (hashmap_iter(module->global_map, &iter, &item)) {
        DaiPropertyOffset* offset = item;
        assert(offset->offset < count);
        offsets[offset->offset] = *offset;
    }
    // 定义到符号表
    for (size_t i = 0; i < count; i++) {
        DaiPropertyOffset* offset = &offsets[i];
        DaiSymbol symbol          = DaiSymbolTable_define(
            symbol_table, offset->property->chars, offset->offset < BUILTIN_GLOBALS_COUNT);
        assert(symbol.index == offset->offset);
    }
    // 释放内存
    free(offsets);
    DaiSymbolTable_setOuter(symbol_table, module->vm->builtinSymbolTable);
}

void
DaiObjModule_afterCompile(DaiObjModule* module, DaiSymbolTable* symbol_table) {
    assert(!(module->compiled));
    module->compiled = true;
    // 按实际的全局变量数量重新分配内存
    int count = DaiSymbolTable_count(symbol_table);
    assert(count >= BUILTIN_GLOBALS_COUNT);
    module->globals = realloc(module->globals, sizeof(DaiValue) * count);
    if (module->globals == NULL) {
        dai_error("realloc globals(%zu bytes) error\n", count * sizeof(DaiValue));
        abort();
    }

    // 迭代符号表
    DaiSymbolTable_iter(symbol_table);
    DaiSymbol symbol;
    while (DaiSymbolTable_next(symbol_table, &symbol)) {
        assert(symbol.type == DaiSymbolType_global && symbol.index < count);
        DaiPropertyOffset offset = {
            .property = dai_copy_string_intern(module->vm, symbol.name, strlen(symbol.name)),
            .offset   = symbol.index,
        };
        const void* res = hashmap_set(module->global_map, &offset);
        if (res == NULL && hashmap_oom(module->global_map)) {
            dai_error("DaiObjModule_afterCompile: Out of memory\n");
            abort();
        }
        if (res == NULL) {
            module->globals[symbol.index] = UNDEFINED_VAL;
        } else {
            assert(symbol.index == ((DaiPropertyOffset*)res)->offset);
        }
    }
}

bool
DaiObjModule_get_global(DaiObjModule* module, const char* name, DaiValue* value) {
    assert(module->compiled);
    DaiObjString* property = dai_find_string_intern(module->vm, name, strlen(name));
    return DaiObjModule_get_global1(module, property, value);
}

bool
DaiObjModule_set_global(DaiObjModule* module, const char* name, DaiValue value) {
    assert(module->compiled);
    DaiObjString* property = dai_find_string_intern(module->vm, name, strlen(name));
    assert(property != NULL);
    const DaiPropertyOffset* offset = hashmap_get(
        module->global_map,
        &(DaiPropertyOffset){.property = property, .offset = module->predefine_global_count});
    if (offset == NULL) {
        return false;
    }
    module->globals[offset->offset] = value;
    return true;
}

bool
DaiObjModule_add_global(DaiObjModule* module, const char* name, DaiValue value) {
    assert(!(module->compiled));
    DaiObjString* property   = dai_copy_string_intern(module->vm, name, strlen(name));
    DaiPropertyOffset offset = {
        .property = property,
        .offset   = module->predefine_global_count,
    };
    // 先检查是否已经存在
    const void* res = hashmap_get(module->global_map, &offset);
    if (res != NULL) {
        return false;
    }
    if (hashmap_set(module->global_map, &offset) == NULL && hashmap_oom(module->global_map)) {
        dai_error("DaiObjModule_New: Out of memory\n");
        abort();
    }
    module->globals[module->predefine_global_count] = value;
    module->predefine_global_count++;
    return true;
}

bool
DaiObjModule_iter(DaiObjModule* module, size_t* i, DaiValue* key, DaiValue* value) {
    void* item;
    if (hashmap_iter(module->global_map, i, &item)) {
        DaiPropertyOffset offset = *(DaiPropertyOffset*)item;
        *key                     = OBJ_VAL(offset.property);
        *value                   = module->globals[offset.offset];
        return true;
    }
    return false;
}
// #endregion

// #region function

static char*
DaiObjFunction_String(DaiValue value, __attribute__((unused)) DaiPtrArray* visited) {
    DaiObjFunction* function = AS_FUNCTION(value);
    int size                 = function->name->length + 16;
    char* buf                = ALLOCATE(char, size);
    snprintf(buf, size, "<fn %s>", function->name->chars);
    return buf;
}

static struct DaiOperation function_operation = {
    .get_property_func  = dai_default_get_property,
    .set_property_func  = dai_default_set_property,
    .subscript_get_func = dai_default_subscript_get,
    .subscript_set_func = dai_default_subscript_set,
    .string_func        = DaiObjFunction_String,
    .equal_func         = dai_default_equal,
    .hash_func          = dai_default_hash,
    .iter_init_func     = NULL,
    .iter_next_func     = NULL,
    .get_method_func    = dai_default_get_method,
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

static struct DaiOperation closure_operation = {
    .get_property_func  = dai_default_get_property,
    .set_property_func  = dai_default_set_property,
    .subscript_get_func = dai_default_subscript_get,
    .subscript_set_func = dai_default_subscript_set,
    .string_func        = DaiObjClosure_String,
    .equal_func         = dai_default_equal,
    .hash_func          = dai_default_hash,
    .iter_init_func     = NULL,
    .iter_next_func     = NULL,
    .get_method_func    = dai_default_get_method,
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

// #region tuple

static DaiValue
DaiObjTuple_subscript_get(__attribute__((unused)) DaiVM* vm, DaiValue receiver, DaiValue index) {
    assert(IS_TUPLE(receiver));
    if (!IS_INTEGER(index)) {
        DaiObjError* err = DaiObjError_Newf(vm, "tuple index must be integer");
        return OBJ_VAL(err);
    }
    const DaiObjTuple* tuple = AS_TUPLE(receiver);
    int64_t n                = AS_INTEGER(index);
    if (n < 0) {
        n += tuple->values.count;
    }
    if (n < 0 || n >= tuple->values.count) {
        DaiObjError* err = DaiObjError_Newf(vm, "tuple index out of range");
        return OBJ_VAL(err);
    }
    return tuple->values.values[n];
}

static char*
DaiObjTuple_String(DaiValue value, __attribute__((unused)) DaiPtrArray* visited) {
    if (DaiPtrArray_contains(visited, AS_OBJ(value))) {
        return strdup("(...)");
    }
    DaiPtrArray_write(visited, AS_OBJ(value));
    DaiStringBuffer* sb = DaiStringBuffer_New();
    DaiObjArray* array  = AS_ARRAY(value);
    DaiStringBuffer_write(sb, "(");
    for (int i = 0; i < array->length; i++) {
        char* s = dai_value_string_with_visited(array->elements[i], visited);
        DaiStringBuffer_write(sb, s);
        free(s);
        if (i != array->length - 1) {
            DaiStringBuffer_write(sb, ", ");
        }
    }
    DaiStringBuffer_write(sb, ")");
    return DaiStringBuffer_getAndFree(sb, NULL);
}

static struct DaiOperation tuple_operation = {
    .get_property_func  = dai_default_get_property,
    .set_property_func  = dai_default_set_property,
    .subscript_get_func = DaiObjTuple_subscript_get,
    .subscript_set_func = dai_default_subscript_set,
    .string_func        = DaiObjTuple_String,
    .equal_func         = dai_default_equal,
    .hash_func          = dai_default_hash,
    .iter_init_func     = NULL,
    .iter_next_func     = NULL,
    .get_method_func    = dai_default_get_method,
};

DaiObjTuple*
DaiObjTuple_New(DaiVM* vm) {
    DaiObjTuple* tuple   = ALLOCATE_OBJ(vm, DaiObjTuple, DaiObjType_tuple);
    tuple->obj.operation = &tuple_operation;
    DaiValueArray_init(&tuple->values);
    return tuple;
}

void
DaiObjTuple_Free(DaiVM* vm, DaiObjTuple* tuple) {
    DaiValueArray_reset(&tuple->values);
    VM_FREE(vm, DaiObjTuple, tuple);
}
void
DaiObjTuple_append(DaiObjTuple* tuple, DaiValue value) {
    DaiValueArray_write(&tuple->values, value);
}

int
DaiObjTuple_length(DaiObjTuple* tuple) {
    return tuple->values.count;
}

DaiValue
DaiObjTuple_get(DaiObjTuple* tuple, int index) {
    assert(index >= 0 && index < tuple->values.count);
    return tuple->values.values[index];
}

static void
DaiObjTuple_set(DaiObjTuple* tuple, int index, DaiValue value) {
    assert(index >= 0 && index < tuple->values.count);
    tuple->values.values[index] = value;
}
// #endregion

// #region 类与实例

static int
DaiFieldDesc_compare(const void* a, const void* b, void* udata) {
    const DaiFieldDesc* pa = a;
    const DaiFieldDesc* pb = b;
    if (pa->name == pb->name) {
        return 0;
    }
    return strcmp(pa->name->chars, pb->name->chars);
}

static uint64_t
DaiFieldDesc_hash(const void* item, uint64_t seed0, uint64_t seed1) {
    const DaiFieldDesc* p = item;
    return p->name->hash;
}

static bool
DaiObjInstance_get_method1(DaiVM* vm, DaiObjClass* klass, DaiObjString* name, DaiValue* method) {
    while (klass != NULL) {
        if (DaiTable_get(&klass->methods, name, method)) {
            return true;
        }
        // 继续向上查找
        klass = klass->parent;
    }
    return false;
}


static DaiValue
DaiObjInstance_get_property(DaiVM* vm, DaiValue receiver, DaiObjString* name) {
    assert(IS_OBJ(receiver));
    DaiObjInstance* instance = AS_INSTANCE(receiver);
    DaiFieldDesc prop        = {.name = name};
    const void* res          = hashmap_get_with_hash(instance->klass->fields, &prop, name->hash);
    if (res) {
        return instance->fields[((DaiFieldDesc*)res)->index];
    }

    DaiValue value;
    if (DaiObjInstance_get_method1(vm, instance->klass, name, &value)) {
        DaiObjBoundMethod* bound_method = DaiObjBoundMethod_New(vm, receiver, value);
        return OBJ_VAL(bound_method);
    }

    DaiObjError* err = DaiObjError_Newf(
        vm, "'%s' object has not property '%s'", dai_object_ts(receiver), name->chars);
    return OBJ_VAL(err);
}

static DaiValue
DaiObjInstance_set_property(DaiVM* vm, DaiValue receiver, DaiObjString* name, DaiValue value) {
    assert(IS_OBJ(receiver));
    DaiObjInstance* instance = AS_INSTANCE(receiver);
    DaiFieldDesc prop        = {.name = name};
    const void* res          = hashmap_get_with_hash(instance->klass->fields, &prop, name->hash);
    if (res) {
        const DaiFieldDesc* propp = res;
        if (!(instance->initialized && propp->is_const)) {
            instance->fields[propp->index] = value;
        } else {
            // 不能修改常量属性
            DaiObjError* err = DaiObjError_Newf(vm,
                                                "'%s' object can not set const property '%s'",
                                                dai_object_ts(receiver),
                                                name->chars);
            return OBJ_VAL(err);
        }
        return NIL_VAL;
    }
    DaiObjError* err = DaiObjError_Newf(
        vm, "'%s' object has not property '%s'", dai_object_ts(receiver), name->chars);
    return OBJ_VAL(err);
}

static bool
DaiObjClass_get_method1(DaiVM* vm, DaiObjClass* klass, DaiObjString* name, DaiValue* method) {
    while (klass != NULL) {
        if (DaiTable_get(&klass->class_methods, name, method)) {
            return true;
        }
        // 继续向上查找
        klass = klass->parent;
    }
    return false;
}

static DaiValue
DaiObjClass_get_property(DaiVM* vm, DaiValue receiver, DaiObjString* name) {
    assert(IS_OBJ(receiver));
    DaiObjClass* klass = AS_CLASS(receiver);
    DaiFieldDesc prop  = {.name = name};
    const void* res    = hashmap_get_with_hash(klass->class_fields, &prop, name->hash);
    if (res) {
        return ((DaiFieldDesc*)res)->value;
    }
    DaiValue value;
    if (DaiObjClass_get_method1(vm, klass, name, &value)) {
        DaiObjBoundMethod* bound_method = DaiObjBoundMethod_New(vm, receiver, value);
        return OBJ_VAL(bound_method);
    }

    DaiObjError* err = DaiObjError_Newf(
        vm, "'%s' object has not property '%s'", dai_object_ts(receiver), name->chars);
    return OBJ_VAL(err);
}

static DaiValue
DaiObjClass_set_property(DaiVM* vm, DaiValue receiver, DaiObjString* name, DaiValue value) {
    assert(IS_OBJ(receiver));
    DaiObjClass* klass = AS_CLASS(receiver);
    DaiFieldDesc prop  = {.name = name};
    const void* res    = hashmap_get_with_hash(klass->class_fields, &prop, name->hash);
    if (res) {
        const DaiFieldDesc* propp = res;
        if (!(propp->is_const)) {
            DaiFieldDesc nprop = {
                .name     = name,
                .value    = value,
                .is_const = propp->is_const,
            };
            res = hashmap_set_with_hash(klass->class_fields, &nprop, name->hash);
            assert(res != NULL);
        } else {
            // 不能修改常量属性
            DaiObjError* err = DaiObjError_Newf(vm,
                                                "'%s' object can not set const property '%s'",
                                                dai_object_ts(receiver),
                                                name->chars);
            return OBJ_VAL(err);
        }
        return NIL_VAL;
    }

    DaiObjError* err = DaiObjError_Newf(
        vm, "'%s' object has not property '%s'", dai_object_ts(receiver), name->chars);
    return OBJ_VAL(err);
}

static char*
DaiObjClass_String(DaiValue value, __attribute__((unused)) DaiPtrArray* visited) {
    DaiObjClass* klass = AS_CLASS(value);
    int size           = klass->name->length + 16;
    char* buf          = ALLOCATE(char, size);
    snprintf(buf, size, "<class %s>", klass->name->chars);
    return buf;
}

static DaiValue
DaiObjClass_get_method(DaiVM* vm, DaiValue receiver, DaiObjString* name) {
    assert(IS_OBJ(receiver));
    DaiObjClass* klass = AS_CLASS(receiver);
    DaiValue value;
    if (DaiObjClass_get_method1(vm, klass, name, &value)) {
        return value;
    }
    DaiObjError* err = DaiObjError_Newf(
        vm, "'%s' object has not method '%s'", dai_object_ts(receiver), name->chars);
    return OBJ_VAL(err);
}

static struct DaiOperation class_operation = {
    .get_property_func  = DaiObjClass_get_property,
    .set_property_func  = DaiObjClass_set_property,
    .subscript_get_func = dai_default_subscript_get,
    .subscript_set_func = dai_default_subscript_set,
    .string_func        = DaiObjClass_String,
    .equal_func         = dai_default_equal,
    .hash_func          = dai_default_hash,
    .iter_init_func     = NULL,
    .iter_next_func     = NULL,
    .get_method_func    = DaiObjClass_get_method,
};

#define STRING_NAME(name) dai_copy_string_intern(vm, name, strlen(name))


DaiObjClass*
DaiObjClass_New(DaiVM* vm, DaiObjString* name) {
    DaiObjClass* klass   = ALLOCATE_OBJ(vm, DaiObjClass, DaiObjType_class);
    klass->obj.operation = &class_operation;
    klass->name          = name;

    DaiTable_init(&klass->class_methods);
    DaiTable_init(&klass->methods);

    uint64_t seed0, seed1;
    DaiVM_getSeed2(vm, &seed0, &seed1);
    klass->class_fields = hashmap_new(
        sizeof(DaiFieldDesc), 8, seed0, seed1, DaiFieldDesc_hash, DaiFieldDesc_compare, NULL, vm);
    if (klass->class_fields == NULL) {
        dai_error("DaiObjClass_New: Out of memory\n");
        abort();
    }
    klass->fields = hashmap_new(
        sizeof(DaiFieldDesc), 8, seed0, seed1, DaiFieldDesc_hash, DaiFieldDesc_compare, NULL, vm);
    if (klass->fields == NULL) {
        dai_error("DaiObjClass_New: Out of memory\n");
        abort();
    }
    klass->parent             = NULL;
    klass->init_fn            = UNDEFINED_VAL;
    klass->define_field_names = DaiObjTuple_New(vm);

    // 定义内置类属性
    DaiObjClass_define_class_field(klass, STRING_NAME("__name__"), OBJ_VAL(klass->name), true);
    DaiObjClass_define_class_field(
        klass, STRING_NAME("__fields__"), OBJ_VAL(klass->define_field_names), true);
    // 定义内置实例属性
    DaiObjClass_define_field(klass, STRING_NAME("__class__"), OBJ_VAL(klass), true);
    return klass;
}

void
DaiObjClass_Free(DaiVM* vm, DaiObjClass* klass) {
    hashmap_free(klass->class_fields);
    hashmap_free(klass->fields);
    DaiTable_reset(&klass->class_methods);
    DaiTable_reset(&klass->methods);
    VM_FREE(vm, DaiObjClass, klass);
}

DaiValue
DaiObjClass_call(DaiObjClass* klass, DaiVM* vm, int argc, DaiValue* argv) {
    DaiObjInstance* instance = DaiObjInstance_New(vm, klass);
    int define_field_count   = DaiObjTuple_length(klass->define_field_names);
    if (argc > define_field_count) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "Too many arguments, max expected=%d got=%d", define_field_count, argc);
        return OBJ_VAL(err);
    }
    if (IS_UNDEFINED(instance->klass->init_fn)) {
        for (int i = 0; i < argc; i++) {
            DaiObjString* field_name = AS_STRING(DaiObjTuple_get(klass->define_field_names, i));
            const void* res          = hashmap_get_with_hash(
                klass->fields, &(DaiFieldDesc){.name = field_name}, field_name->hash);
            assert(res != NULL);
            instance->fields[((DaiFieldDesc*)res)->index] = argv[i];
        }
    } else if (IS_BUILTINFN(instance->klass->init_fn)) {
        const BuiltinFn func  = AS_BUILTINFN(instance->klass->init_fn)->function;
        const DaiValue result = func(vm, OBJ_VAL(instance), argc, argv);
        if (IS_ERROR(result)) {
            return result;
        }
    } else {
        // 设置此次调用的实例
        vm->stack_top[-argc - 1] = OBJ_VAL(instance);
        DaiValue res             = DaiVM_runCall2(vm, instance->klass->init_fn, argc);
        if (IS_ERROR(res)) {
            return res;
        }
    }
    // check all fields are initialized
    for (int i = argc; i < define_field_count; i++) {
        DaiObjString* field_name = AS_STRING(DaiObjTuple_get(klass->define_field_names, i));
        const void* res          = hashmap_get_with_hash(
            klass->fields, &(DaiFieldDesc){.name = field_name}, field_name->hash);
        assert(res != NULL);
        if (IS_UNDEFINED(instance->fields[((DaiFieldDesc*)res)->index])) {
            DaiObjError* err = DaiObjError_Newf(vm,
                                                "'%s' object has uninitialized field '%s'",
                                                klass->name->chars,
                                                field_name->chars);
            return OBJ_VAL(err);
        }
    }
    instance->initialized = true;
    return OBJ_VAL(instance);
}

void
DaiObjClass_define_class_field(DaiObjClass* klass, DaiObjString* name, DaiValue value,
                               bool is_const) {
    const void* res =
        hashmap_get_with_hash(klass->class_fields, &(DaiFieldDesc){.name = name}, name->hash);
    if (res == NULL) {
        DaiFieldDesc property = {
            .name     = name,
            .is_const = is_const,
            .value    = value,
        };
        res = hashmap_set_with_hash(klass->class_fields, &property, name->hash);
        if (res == NULL && hashmap_oom(klass->class_fields)) {
            dai_error("DaiObjClass_define_class_field: Out of memory\n");
            abort();
        }
    }
}

void
DaiObjClass_define_class_method(DaiObjClass* klass, DaiObjString* name, DaiValue value) {
    // 设置方法的 super class
    {
        if (IS_CLOSURE(value)) {
            DaiObjFunction* function = AS_CLOSURE(value)->function;
            function->superclass     = klass->parent;
        } else if (IS_FUNCTION(value)) {
            DaiObjFunction* function = AS_FUNCTION(value);
            function->superclass     = klass->parent;
        }
    }
    DaiTable_set(&klass->class_methods, name, value);
}

int
DaiObjClass_define_field(DaiObjClass* klass, DaiObjString* name, DaiValue value, bool is_const) {
    const void* res = hashmap_get_with_hash(
        klass->fields, &(DaiPropertyOffset){.property = name, .offset = 0}, name->hash);
    DaiFieldDesc property = {
        .name     = name,
        .is_const = is_const,
        .value    = value,
        .index    = hashmap_count(klass->fields),
    };
    if (res == NULL) {
        if (!is_builtin_property(name->chars)) {
            DaiObjTuple_append(klass->define_field_names, OBJ_VAL(name));
        }
    } else {
        property.index = ((DaiFieldDesc*)res)->index;
    }

    res = hashmap_set_with_hash(klass->fields, &property, name->hash);
    if (res == NULL && hashmap_oom(klass->fields)) {
        dai_error("DaiObjClass_define_field: Out of memory\n");
        abort();
    }
    return property.index;
}

void
DaiObjClass_define_method(DaiObjClass* klass, DaiObjString* name, DaiValue value) {
    // 设置方法的 super class
    {
        if (IS_CLOSURE(value)) {
            DaiObjFunction* function = AS_CLOSURE(value)->function;
            function->superclass     = klass->parent;
        } else if (IS_FUNCTION(value)) {
            DaiObjFunction* function = AS_FUNCTION(value);
            function->superclass     = klass->parent;
        }
    }
    DaiTable_set(&klass->methods, name, value);
    if (strcmp(name->chars, "__init__") == 0) {
        klass->init_fn = value;
    }
}

void
DaiObjClass_inherit(DaiObjClass* klass, DaiObjClass* parent) {
    klass->parent  = parent;
    klass->init_fn = parent->init_fn;
    // 复制实例属性
    {
        void* item;
        size_t i = 0;
        while (hashmap_iter(parent->fields, &i, &item)) {
            DaiFieldDesc* prop = item;
            DaiObjClass_define_field(klass, prop->name, prop->value, prop->is_const);
        }
        int count = DaiObjTuple_length(parent->define_field_names);
        for (int i = 0; i < count; i++) {
            DaiObjTuple_set(
                klass->define_field_names, i, DaiObjTuple_get(parent->define_field_names, i));
        }
    }
    // 复制类属性
    {
        void* item;
        size_t i = 0;
        while (hashmap_iter(parent->class_fields, &i, &item)) {
            DaiFieldDesc* prop = item;
            DaiObjClass_define_class_field(klass, prop->name, prop->value, prop->is_const);
        }
    }
}

static char*
DaiObjInstance_String(DaiValue value, __attribute__((unused)) DaiPtrArray* visited) {
    DaiObjInstance* instance = AS_INSTANCE(value);
    int size                 = instance->klass->name->length + 64;
    char* buf                = ALLOCATE(char, size);
    snprintf(buf, size, "<instance of %s at %p>", instance->klass->name->chars, instance);
    return buf;
}

static DaiValue
DaiObjInstance_get_method(DaiVM* vm, DaiValue receiver, DaiObjString* name) {
    DaiValue method;
    DaiObjInstance* instance = AS_INSTANCE(receiver);
    if (DaiObjInstance_get_method1(vm, instance->klass, name, &method)) {
        return method;
    }
    DaiObjError* err = DaiObjError_Newf(
        vm, "'%s' object has not method '%s'", dai_object_ts(OBJ_VAL(instance)), name->chars);
    return OBJ_VAL(err);
}

static struct DaiOperation instance_operation = {
    .get_property_func  = DaiObjInstance_get_property,
    .set_property_func  = DaiObjInstance_set_property,
    .subscript_get_func = dai_default_subscript_get,
    .subscript_set_func = dai_default_subscript_set,
    .string_func        = DaiObjInstance_String,
    .equal_func         = dai_default_equal,
    .hash_func          = dai_default_hash,
    .iter_init_func     = NULL,
    .iter_next_func     = NULL,
    .get_method_func    = DaiObjInstance_get_method,
};


DaiObjInstance*
DaiObjInstance_New(DaiVM* vm, DaiObjClass* klass) {
    DaiObjInstance* instance = ALLOCATE_OBJ(vm, DaiObjInstance, DaiObjType_instance);
    instance->obj.operation  = &instance_operation;
    instance->klass          = klass;
    instance->initialized    = false;
    instance->field_count    = hashmap_count(klass->fields);
    instance->fields         = NULL;
    if (instance->field_count > 0) {
        instance->fields = malloc(sizeof(DaiValue) * instance->field_count);
        if (instance->fields == NULL) {
            dai_error("DaiObjInstance_New: Out of memory\n");
            abort();
        }
    }
    void* item;
    size_t i = 0;
    while (hashmap_iter(klass->fields, &i, &item)) {
        DaiFieldDesc* prop            = item;
        instance->fields[prop->index] = prop->value;
    }
    return instance;
}

void
DaiObjInstance_Free(DaiVM* vm, DaiObjInstance* instance) {
    free(instance->fields);
    instance->fields = NULL;
    VM_FREE(vm, DaiObjInstance, instance);
}

static char*
DaiObjBoundMethod_String(DaiValue value, __attribute__((unused)) DaiPtrArray* visited) {
    DaiObjBoundMethod* bound_method = AS_BOUND_METHOD(value);
    char* instance_str              = dai_value_string(bound_method->receiver);
    size_t size = strlen(instance_str) + bound_method->method->function->name->length + 32;
    char* buf   = ALLOCATE(char, size);
    snprintf(buf,
             size,
             "<bound method %s of %s>",
             bound_method->method->function->name->chars,
             instance_str);
    FREE_ARRAY(char, instance_str, strlen(instance_str) + 1);
    return buf;
}

static struct DaiOperation bound_method_operation = {
    .get_property_func  = dai_default_get_property,
    .set_property_func  = dai_default_set_property,
    .subscript_get_func = dai_default_subscript_get,
    .subscript_set_func = dai_default_subscript_set,
    .string_func        = DaiObjBoundMethod_String,
    .equal_func         = dai_default_equal,
    .hash_func          = dai_default_hash,
    .iter_init_func     = NULL,
    .iter_next_func     = NULL,
    .get_method_func    = dai_default_get_method,
};

DaiObjBoundMethod*
DaiObjBoundMethod_New(DaiVM* vm, DaiValue receiver, DaiValue method) {
    DaiObjBoundMethod* bound_method = ALLOCATE_OBJ(vm, DaiObjBoundMethod, DaiObjType_boundMethod);
    bound_method->obj.operation     = &bound_method_operation;
    bound_method->receiver          = receiver;
    bound_method->method            = AS_CLOSURE(method);
    return bound_method;
}
DaiObjBoundMethod*
DaiObjClass_get_super_method(DaiVM* vm, DaiObjClass* klass, DaiObjString* name, DaiValue receiver) {
    DaiValue method;
    if (IS_CLASS(receiver)) {
        if (DaiObjClass_get_method1(vm, klass, name, &method)) {
            return DaiObjBoundMethod_New(vm, receiver, method);
        }
    } else {
        if (DaiObjInstance_get_method1(vm, klass, name, &method)) {
            return DaiObjBoundMethod_New(vm, receiver, method);
        }
    }
    return NULL;
}
void
DaiObj_get_method(DaiVM* vm, DaiObjClass* klass, DaiValue receiver, DaiObjString* name,
                  DaiValue* method) {
    if (IS_CLASS(receiver)) {
        if (DaiObjClass_get_method1(vm, klass, name, method)) {
            return;
        }
    } else {
        if (DaiObjInstance_get_method1(vm, klass, name, method)) {
            return;
        }
    }
    DaiObjError* err = DaiObjError_Newf(vm, "'super' object has not property '%s'", name->chars);
    *method          = OBJ_VAL(err);
}
// #endregion

// #region 字符串

static int
utf8_one_char_length(const char* s) {
    // code copy from https://github.com/sheredom/utf8.h
    if (0xf0 == (0xf8 & *s)) {
        /* 4-byte utf8 code point (began with 0b11110xxx) */
        return 4;
    } else if (0xe0 == (0xf0 & *s)) {
        /* 3-byte utf8 code point (began with 0b1110xxxx) */
        return 3;
    } else if (0xc0 == (0xe0 & *s)) {
        /* 2-byte utf8 code point (began with 0b110xxxxx) */
        return 2;
    } else { /* if (0x00 == (0x80 & *s)) { */
        /* 1-byte ascii (began with 0b0xxxxxxx) */
        return 1;
    }
}

/**
 * @brief 返回字符串的第n个字符的指针
 *
 * @note 该函数不检查n是否越界, 请确保n不会越界
 *
 * @param str 字符串
 * @param n 第n个
 *
 * @return char*
 */
char*
utf8offset(const char* str, int n) {
    while (n > 0) {
        str += utf8_one_char_length(str);
        n--;
    }
    return (char*)str;
}

static DaiValue
DaiObjString_length(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc,
                    DaiValue* argv) {
    if (argc != 0) {
        DaiObjError* err = DaiObjError_Newf(vm, "length() expected no arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    return INTEGER_VAL(AS_STRING(receiver)->utf8_length);
}

static DaiValue
DaiObjString_format(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc,
                    DaiValue* argv) {
    DaiStringBuffer* sb  = DaiStringBuffer_New();
    DaiObjString* string = AS_STRING(receiver);
    const char* format   = string->chars;
    int i                = 0;
    int j                = 0;
    while (format[i] != '\0') {
        if (format[i] == '{' && format[i + 1] == '}') {
            if (j >= argc) {
                DaiStringBuffer_free(sb);
                DaiObjError* err = DaiObjError_Newf(vm, "format() not enough arguments");
                return OBJ_VAL(err);
            }
            DaiValue value = argv[j];
            char* s        = dai_value_string(value);
            DaiStringBuffer_write(sb, s);
            free(s);
            i += 2;
            j++;
        } else {
            int step = utf8_one_char_length(format + i);
            DaiStringBuffer_writen(sb, format + i, step);
            i += step;
        }
    }
    if (j != argc) {
        DaiStringBuffer_free(sb);
        DaiObjError* err = DaiObjError_Newf(vm, "format() too many arguments");
        return OBJ_VAL(err);
    }
    size_t length = 0;
    char* res     = DaiStringBuffer_getAndFree(sb, &length);
    return OBJ_VAL(dai_take_string(vm, res, length));
}

static DaiValue
DaiObjString_sub(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc == 0 || argc > 2) {
        DaiObjError* err = DaiObjError_Newf(vm, "sub() expected 1-2 arguments, but got 0");
        return OBJ_VAL(err);
    }
    if (!IS_INTEGER(argv[0])) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "sub() expected int arguments, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
    if (argc == 2 && !IS_INTEGER(argv[1])) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "sub() expected int arguments, but got %s", dai_value_ts(argv[1]));
        return OBJ_VAL(err);
    }
    DaiObjString* string = AS_STRING(receiver);
    int start            = AS_INTEGER(argv[0]);
    int end              = argc == 1 ? string->utf8_length : AS_INTEGER(argv[1]);
    if (start < 0) {
        start += string->utf8_length;
        if (start < 0) {
            start = 0;
        }
    }
    if (end < 0) {
        end += string->utf8_length;
    } else if (end > string->utf8_length) {
        end = string->utf8_length;
    }
    if (start >= end) {
        return OBJ_VAL(dai_copy_string(vm, "", 0));
    }
    char* s = utf8offset(string->chars, start);
    char* e = utf8offset(s, end - start);
    return OBJ_VAL(dai_copy_string(vm, s, e - s));
}

static DaiValue
DaiObjString_find(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err = DaiObjError_Newf(vm, "find() expected 1 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (!IS_STRING(argv[0])) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "find() expected string arguments, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
    DaiObjString* string = AS_STRING(receiver);
    DaiObjString* sub    = AS_STRING(argv[0]);
    if (string->length < sub->length) {
        return INTEGER_VAL(-1);
    }
    char* s = strstr(string->chars, sub->chars);
    if (s == NULL) {
        return INTEGER_VAL(-1);
    }
    return INTEGER_VAL(utf8nlen(string->chars, s - string->chars));
}

static char*
DaiObjString_replacen(DaiObjString* s, DaiObjString* old, DaiObjString* new, int max_replacements) {
    DaiStringBuffer* sb = DaiStringBuffer_New();
    const char* p       = s->chars;
    const char* end     = s->chars + s->length;
    int old_len         = old->length;
    int new_len         = new->length;
    while (p < end) {
        if (max_replacements == 0) {
            DaiStringBuffer_writen(sb, p, end - p);
            break;
        }
        const char* q = strstr(p, old->chars);
        if (q == NULL) {
            DaiStringBuffer_writen(sb, p, end - p);
            break;
        }
        DaiStringBuffer_writen(sb, p, q - p);
        DaiStringBuffer_writen(sb, new->chars, new_len);
        p = q + old_len;
        max_replacements--;
    }
    size_t length = 0;
    char* res     = DaiStringBuffer_getAndFree(sb, &length);
    return res;
}

static DaiValue
DaiObjString_replace(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc,
                     DaiValue* argv) {
    if ((argc != 2) && (argc != 3)) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "replace() expected 2-3 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (!IS_STRING(argv[0])) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "replace() expected string arguments, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
    if (!IS_STRING(argv[1])) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "replace() expected string arguments, but got %s", dai_value_ts(argv[1]));
        return OBJ_VAL(err);
    }
    if (argc == 3 && !IS_INTEGER(argv[2])) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "replace() expected int arguments, but got %s", dai_value_ts(argv[2]));
        return OBJ_VAL(err);
    }
    DaiObjString* string = AS_STRING(receiver);
    DaiObjString* old    = AS_STRING(argv[0]);
    if (old->length == 0) {
        DaiObjError* err = DaiObjError_Newf(vm, "replace() empty old string");
        return OBJ_VAL(err);
    }
    DaiObjString* new = AS_STRING(argv[1]);
    int count         = argc == 3 ? AS_INTEGER(argv[2]) : INT_MAX;
    char* res         = DaiObjString_replacen(string, old, new, count);
    return OBJ_VAL(dai_take_string(vm, res, strlen(res)));
}

static DaiValue
DaiObjString_splitn(DaiVM* vm, DaiObjString* s, DaiObjString* sep, int max_splits) {
    DaiObjArray* array = DaiObjArray_New(vm, NULL, 0);
    // 临时引用，防止被 GC 回收
    DaiVM_setTempRef(vm, OBJ_VAL(array));
    const char* p   = s->chars;
    const char* end = s->chars + s->length;
    int sep_len     = sep->length;
    while (p < end) {
        if (max_splits <= 0) {
            DaiObjArray_append1(vm, array, 1, &OBJ_VAL(dai_copy_string(vm, p, end - p)));
            break;
        }
        const char* q = strstr(p, sep->chars);
        if (q == NULL) {
            DaiObjArray_append1(vm, array, 1, &OBJ_VAL(dai_copy_string(vm, p, end - p)));
            break;
        }
        DaiObjString* sub = dai_copy_string(vm, p, q - p);
        DaiObjArray_append1(vm, array, 1, &OBJ_VAL(sub));
        p = q + sep_len;
        max_splits--;
    }
    DaiVM_setTempRef(vm, NIL_VAL);
    return OBJ_VAL(array);
}

static DaiValue
DaiObjString_split_whitespace(DaiVM* vm, DaiObjString* s) {
    DaiObjArray* array = DaiObjArray_New(vm, NULL, 0);
    // 临时引用，防止被 GC 回收
    DaiVM_setTempRef(vm, OBJ_VAL(array));
    const char* p   = s->chars;
    const char* end = s->chars + s->length;
    while (p < end) {
        while (p < end && isspace(*p)) {
            p++;
        }
        const char* q = p;
        while (q < end && !isspace(*q)) {
            q++;
        }
        if (p < q) {
            DaiObjString* sub = dai_copy_string(vm, p, q - p);
            DaiObjArray_append1(vm, array, 1, &OBJ_VAL(sub));
        }
        p = q;
    }
    DaiVM_setTempRef(vm, NIL_VAL);
    return OBJ_VAL(array);
}

static DaiValue
DaiObjString_split(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc == 0) {
        return DaiObjString_split_whitespace(vm, AS_STRING(receiver));
    }
    if ((argc != 1) && (argc != 2)) {
        DaiObjError* err = DaiObjError_Newf(vm, "split() expected 0-2 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (!IS_STRING(argv[0])) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "split() expected string arguments, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
    if (argc == 2 && !IS_INTEGER(argv[1])) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "split() expected int arguments, but got %s", dai_value_ts(argv[1]));
        return OBJ_VAL(err);
    }
    DaiObjString* string = AS_STRING(receiver);
    DaiObjString* sep    = AS_STRING(argv[0]);
    if (sep->length == 0) {
        DaiObjError* err = DaiObjError_Newf(vm, "split() empty separator");
        return OBJ_VAL(err);
    }
    // split 的 count 参数表示结果数组的最大长度，对应分割次数需要减一
    int count = argc == 2 ? AS_INTEGER(argv[1]) - 1 : INT_MAX;
    return DaiObjString_splitn(vm, string, sep, count);
}

static DaiValue
DaiObjString_join(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err = DaiObjError_Newf(vm, "join() expected 1 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (!IS_ARRAY(argv[0])) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "join() expected array arguments, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
    DaiObjString* sep  = AS_STRING(receiver);
    DaiObjArray* array = AS_ARRAY(argv[0]);
    if (array->length == 0) {
        return OBJ_VAL(dai_copy_string(vm, "", 0));
    }
    DaiStringBuffer* sb = DaiStringBuffer_New();
    for (int i = 0; i < array->length; i++) {
        if (!IS_STRING(array->elements[i])) {
            DaiStringBuffer_free(sb);
            DaiObjError* err =
                DaiObjError_Newf(vm,
                                 "join() expected array of string arguments, but got %s at %d",
                                 dai_value_ts(array->elements[i]),
                                 i);
            return OBJ_VAL(err);
        }
        DaiStringBuffer_write(sb, AS_STRING(array->elements[i])->chars);
        if (i != array->length - 1) {
            DaiStringBuffer_write(sb, sep->chars);
        }
    }
    size_t length = 0;
    char* res     = DaiStringBuffer_getAndFree(sb, &length);
    return OBJ_VAL(dai_take_string(vm, res, length));
}

static DaiValue
DaiObjString_has(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err = DaiObjError_Newf(vm, "has() expected 1 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (!IS_STRING(argv[0])) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "has() expected string arguments, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
    DaiObjString* string = AS_STRING(receiver);
    DaiObjString* sub    = AS_STRING(argv[0]);
    char* s              = strstr(string->chars, sub->chars);
    return BOOL_VAL(s != NULL);
}

static DaiValue
DaiObjString_strip(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 0) {
        DaiObjError* err = DaiObjError_Newf(vm, "strip() expected no arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    DaiObjString* string = AS_STRING(receiver);
    const char* p        = string->chars;
    const char* end      = string->chars + string->length;
    while (p < end && isspace(*p)) {
        p++;
    }
    while (end > p && isspace(*(end - 1))) {
        end--;
    }
    if (p == string->chars && end == string->chars + string->length) {
        return receiver;
    }
    return OBJ_VAL(dai_copy_string(vm, p, end - p));
}

static DaiValue
DaiObjString_startswith(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc,
                        DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "startswith() expected 1 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (!IS_STRING(argv[0])) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "startswith() expected string arguments, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
    DaiObjString* string = AS_STRING(receiver);
    DaiObjString* sub    = AS_STRING(argv[0]);
    return BOOL_VAL(strncmp(string->chars, sub->chars, sub->length) == 0);
}

static DaiValue
DaiObjString_endswith(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc,
                      DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "endswith() expected 1 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (!IS_STRING(argv[0])) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "endswith() expected string arguments, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
    DaiObjString* string = AS_STRING(receiver);
    DaiObjString* sub    = AS_STRING(argv[0]);
    return BOOL_VAL(
        strncmp(string->chars + string->length - sub->length, sub->chars, sub->length) == 0);
}

enum DaiObjStringFunctionNo {
    DaiObjStringFunctionNo_length = 0,
    DaiObjStringFunctionNo_format,
    DaiObjStringFunctionNo_sub,
    DaiObjStringFunctionNo_find,
    DaiObjStringFunctionNo_replace,
    DaiObjStringFunctionNo_split,
    DaiObjStringFunctionNo_join,
    DaiObjStringFunctionNo_has,
    DaiObjStringFunctionNo_strip,
    DaiObjStringFunctionNo_startswith,
    DaiObjStringFunctionNo_endswith,
};

static DaiObjBuiltinFunction DaiObjStringBuiltins[] = {
    [DaiObjStringFunctionNo_length] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "length",
            .function = &DaiObjString_length,
        },
    [DaiObjStringFunctionNo_format] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "format",
            .function = &DaiObjString_format,
        },
    [DaiObjStringFunctionNo_sub] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "sub",
            .function = &DaiObjString_sub,
        },
    [DaiObjStringFunctionNo_find] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "find",
            .function = &DaiObjString_find,
        },
    [DaiObjStringFunctionNo_replace] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "replace",
            .function = &DaiObjString_replace,
        },
    [DaiObjStringFunctionNo_split] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "split",
            .function = &DaiObjString_split,
        },
    [DaiObjStringFunctionNo_join] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "join",
            .function = &DaiObjString_join,
        },
    [DaiObjStringFunctionNo_has] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "has",
            .function = &DaiObjString_has,
        },
    [DaiObjStringFunctionNo_strip] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "strip",
            .function = &DaiObjString_strip,
        },
    [DaiObjStringFunctionNo_startswith] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "startswith",
            .function = &DaiObjString_startswith,
        },
    [DaiObjStringFunctionNo_endswith] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "endswith",
            .function = &DaiObjString_endswith,
        },
};

static DaiValue
DaiObjString_get_property(DaiVM* vm, DaiValue receiver, DaiObjString* name) {
    const char* cname = name->chars;
    if (strcmp(cname, "length") == 0) {
        return OBJ_VAL(&DaiObjStringBuiltins[DaiObjStringFunctionNo_length]);
    }
    if (strcmp(cname, "format") == 0) {
        return OBJ_VAL(&DaiObjStringBuiltins[DaiObjStringFunctionNo_format]);
    }
    if (strcmp(cname, "sub") == 0) {
        return OBJ_VAL(&DaiObjStringBuiltins[DaiObjStringFunctionNo_sub]);
    }
    if (strcmp(cname, "find") == 0) {
        return OBJ_VAL(&DaiObjStringBuiltins[DaiObjStringFunctionNo_find]);
    }
    if (strcmp(cname, "replace") == 0) {
        return OBJ_VAL(&DaiObjStringBuiltins[DaiObjStringFunctionNo_replace]);
    }
    if (strcmp(cname, "split") == 0) {
        return OBJ_VAL(&DaiObjStringBuiltins[DaiObjStringFunctionNo_split]);
    }
    if (strcmp(cname, "join") == 0) {
        return OBJ_VAL(&DaiObjStringBuiltins[DaiObjStringFunctionNo_join]);
    }
    if (strcmp(cname, "has") == 0) {
        return OBJ_VAL(&DaiObjStringBuiltins[DaiObjStringFunctionNo_has]);
    }
    if (strcmp(cname, "strip") == 0) {
        return OBJ_VAL(&DaiObjStringBuiltins[DaiObjStringFunctionNo_strip]);
    }
    if (strcmp(cname, "startswith") == 0) {
        return OBJ_VAL(&DaiObjStringBuiltins[DaiObjStringFunctionNo_startswith]);
    }
    if (strcmp(cname, "endswith") == 0) {
        return OBJ_VAL(&DaiObjStringBuiltins[DaiObjStringFunctionNo_endswith]);
    }
    DaiObjError* err = DaiObjError_Newf(
        vm, "'%s' object has not property '%s'", dai_object_ts(receiver), name->chars);
    return OBJ_VAL(err);
}

static char*
DaiObjString_String(DaiValue value, __attribute__((unused)) DaiPtrArray* visited) {
    return strdup(AS_STRING(value)->chars);
}

static DaiValue
DaiObjString_subscript_get(DaiVM* vm, DaiValue receiver, DaiValue index) {
    if (!IS_INTEGER(index)) {
        DaiObjError* err = DaiObjError_Newf(vm, "index must be integer");
        return OBJ_VAL(err);
    }
    DaiObjString* string = AS_STRING(receiver);
    int i                = AS_INTEGER(index);
    if (i < 0) {
        i += string->utf8_length;
    }
    if (i < 0 || i >= string->utf8_length) {
        DaiObjError* err = DaiObjError_Newf(vm, "index out of range");
        return OBJ_VAL(err);
    }
    char* s = utf8offset(string->chars, i);
    return OBJ_VAL(dai_copy_string(vm, s, utf8_one_char_length(s)));
}

static DaiValue
DaiObjString_subscript_set(__attribute__((unused)) DaiVM* vm, DaiValue receiver, DaiValue index,
                           DaiValue value) {
    DaiObjError* err = DaiObjError_Newf(vm, "'string' object does not support item assignment");
    return OBJ_VAL(err);
}

static int
DaiObjString_equal(DaiValue a, DaiValue b, __attribute__((unused)) int* limit) {
    DaiObjString* sa = AS_STRING(a);
    DaiObjString* sb = AS_STRING(b);
    return (sa == sb) ||
           (sa->length == sb->length && strncmp(sa->chars, sb->chars, sa->length) == 0);
}

static uint64_t
DaiObjString_hash(DaiValue value) {
    return AS_STRING(value)->hash;
}

static struct DaiOperation string_operation = {
    .get_property_func  = DaiObjString_get_property,
    .set_property_func  = dai_default_set_property,
    .subscript_get_func = DaiObjString_subscript_get,
    .subscript_set_func = DaiObjString_subscript_set,
    .string_func        = DaiObjString_String,
    .equal_func         = DaiObjString_equal,
    .hash_func          = DaiObjString_hash,
    .iter_init_func     = NULL,
    .iter_next_func     = NULL,
    .get_method_func    = DaiObjString_get_property,
};


static DaiObjString*
allocate_string(DaiVM* vm, char* chars, int length, uint32_t hash) {
    DaiObjString* string  = ALLOCATE_OBJ(vm, DaiObjString, DaiObjType_string);
    string->length        = length;
    string->utf8_length   = utf8len(chars);
    string->chars         = chars;
    string->hash          = hash;
    string->obj.operation = &string_operation;
    DaiTable_set(&vm->strings, string, NIL_VAL);
    return string;
}
static uint32_t
hash_string(const char* key, int length) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}
__attribute__((unused)) DaiObjString*
dai_take_string_intern(DaiVM* vm, char* chars, int length) {
    uint32_t hash          = hash_string(chars, length);
    DaiObjString* interned = DaiTable_findString(&vm->strings, chars, length, hash);
    if (interned != NULL) {
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }

    return allocate_string(vm, chars, length, hash);
}
DaiObjString*
dai_copy_string_intern(DaiVM* vm, const char* chars, int length) {
    uint32_t hash          = hash_string(chars, length);
    DaiObjString* interned = DaiTable_findString(&vm->strings, chars, length, hash);
    if (interned != NULL) return interned;

    char* heap_chars = VM_ALLOCATE(vm, char, length + 1);
    memcpy(heap_chars, chars, length);
    heap_chars[length] = '\0';
    return allocate_string(vm, heap_chars, length, hash);
}

static DaiObjString*
dai_find_string_intern(DaiVM* vm, const char* chars, int length) {
    uint32_t hash = hash_string(chars, length);
    return DaiTable_findString(&vm->strings, chars, length, hash);
}

DaiObjString*
dai_take_string(DaiVM* vm, char* chars, int length) {
    uint32_t hash = hash_string(chars, length);
    return allocate_string(vm, chars, length, hash);
}

DaiObjString*
dai_copy_string(DaiVM* vm, const char* chars, int length) {
    uint32_t hash    = hash_string(chars, length);
    char* heap_chars = VM_ALLOCATE(vm, char, length + 1);
    memcpy(heap_chars, chars, length);
    heap_chars[length] = '\0';
    return allocate_string(vm, heap_chars, length, hash);
}

// Function to compare two DaiObjString objects
int
DaiObjString_cmp(DaiObjString* a, DaiObjString* b) {
    // Compare the lengths first
    if (a->length < b->length) return -1;
    if (a->length > b->length) return 1;

    // If lengths are equal, compare the strings lexicographically
    return strcmp(a->chars, b->chars);
}
// #endregion

// #region 数组 DaiObjArray
static void
DaiObjArray_shrink(DaiObjArray* array) {
    if (array->length > (array->capacity >> 2)) {
        return;
    }
    int old_capacity = array->capacity;
    array->capacity  = array->capacity >> 1;
    array->elements  = GROW_ARRAY(DaiValue, array->elements, old_capacity, array->capacity);
}

static void
DaiObjArray_grow(DaiObjArray* array, int want) {
    int old_capacity = array->capacity;
    array->capacity  = GROW_CAPACITY(array->capacity);
    if (want > array->capacity) {
        array->capacity = want;
    }
    array->elements = GROW_ARRAY(DaiValue, array->elements, old_capacity, array->capacity);
}

static DaiObjArray*
DaiObjArray_pcopy(DaiVM* vm, DaiObjArray* array) {
    DaiObjArray* copy = DaiObjArray_New(vm, NULL, 0);
    copy->length      = array->length;
    copy->capacity    = array->capacity;
    copy->elements    = GROW_ARRAY(DaiValue, NULL, 0, copy->capacity);
    for (int i = 0; i < array->length; i++) {
        copy->elements[i] = array->elements[i];
    }
    return copy;
}

static void
DaiObjArray_preverse(DaiObjArray* array) {
    for (int i = 0; i < array->length / 2; i++) {
        DaiValue tmp                           = array->elements[i];
        array->elements[i]                     = array->elements[array->length - i - 1];
        array->elements[array->length - i - 1] = tmp;
    }
}

static DaiValue
DaiObjArray_length(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 0) {
        DaiObjError* err = DaiObjError_Newf(vm, "length() expected no arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    return INTEGER_VAL(AS_ARRAY(receiver)->length);
}

static DaiValue
DaiObjArray_append(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc == 0) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "add() expected one or more arguments, but got no arguments");
        return OBJ_VAL(err);
    }
    DaiObjArray* array = AS_ARRAY(receiver);
    DaiObjArray_append1(vm, array, argc, argv);
    return receiver;
}

static DaiValue
DaiObjArray_pop(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 0) {
        DaiObjError* err = DaiObjError_Newf(vm, "pop() expected no arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    DaiObjArray* array = AS_ARRAY(receiver);
    if (array->length == 0) {
        DaiObjError* err = DaiObjError_Newf(vm, "pop from empty array");
        return OBJ_VAL(err);
    }
    DaiValue value = array->elements[array->length - 1];
    array->length--;
    DaiObjArray_shrink(array);
    return value;
}

static DaiValue
DaiObjArray_sub(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 1 && argc != 2) {
        DaiObjError* err = DaiObjError_Newf(vm, "sub() expected 1-2 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (!IS_INTEGER(argv[0])) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "sub() expected int arguments, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
    if (argc == 2 && !IS_INTEGER(argv[1])) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "sub() expected int arguments, but got %s", dai_value_ts(argv[1]));
        return OBJ_VAL(err);
    }
    DaiObjArray* array = AS_ARRAY(receiver);
    int start          = AS_INTEGER(argv[0]);
    int end            = argc == 2 ? AS_INTEGER(argv[1]) : array->length;
    if (start < 0) {
        start += array->length;
        if (start < 0) {
            start = 0;
        }
    }
    if (end < 0) {
        end += array->length;
        if (end < 0) {
            end = 0;
        }
    } else if (end > array->length) {
        end = array->length;
    }
    if (start >= end) {
        return OBJ_VAL(DaiObjArray_New(vm, NULL, 0));
    }
    DaiObjArray* sub_array = DaiObjArray_New(vm, array->elements + start, end - start);
    return OBJ_VAL(sub_array);
}

static DaiValue
DaiObjArray_remove(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err = DaiObjError_Newf(vm, "remove() expected 1 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    DaiObjArray* array = AS_ARRAY(receiver);
    DaiValue value     = argv[0];
    for (int i = 0; i < array->length; i++) {
        int ret = dai_value_equal(array->elements[i], value);
        if (ret == -1) {
            DaiObjError* err =
                DaiObjError_Newf(vm, "maximum recursion depth exceeded in comparison");
            return OBJ_VAL(err);
        }
        if (ret == 1) {
            for (int j = i; j < array->length - 1; j++) {
                array->elements[j] = array->elements[j + 1];
            }
            array->length--;
            DaiObjArray_shrink(array);
            return receiver;
        }
    }
    DaiObjError* err = DaiObjError_Newf(vm, "array.remove(x): x not in array");
    return OBJ_VAL(err);
}

static DaiValue
DaiObjArray_removeIndex(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc,
                        DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "removeIndex() expected 1 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (!IS_INTEGER(argv[0])) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "removeIndex() expected int arguments, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
    DaiObjArray* array = AS_ARRAY(receiver);
    int index          = AS_INTEGER(argv[0]);
    if (index < 0) {
        index += array->length;
    }
    if (index < 0 || index >= array->length) {
        DaiObjError* err = DaiObjError_Newf(vm, "removeIndex() index out of range");
        return OBJ_VAL(err);
    }
    for (int i = index; i < array->length - 1; i++) {
        array->elements[i] = array->elements[i + 1];
    }
    array->length--;
    DaiObjArray_shrink(array);
    return NIL_VAL;
}

static DaiValue
DaiObjArray_extend(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err = DaiObjError_Newf(vm, "extend() expected 1 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (!IS_ARRAY(argv[0])) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "extend() expected array arguments, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
    DaiObjArray* array = AS_ARRAY(receiver);
    DaiObjArray* other = AS_ARRAY(argv[0]);
    int want           = array->length + other->length;
    if (want > array->capacity) {
        DaiObjArray_grow(array, want);
    }
    for (int i = 0; i < other->length; i++) {
        array->elements[array->length + i] = other->elements[i];
    }
    array->length += other->length;
    return receiver;
}

static DaiValue
DaiObjArray_has(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err = DaiObjError_Newf(vm, "has() expected 1 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    DaiObjArray* array = AS_ARRAY(receiver);
    DaiValue value     = argv[0];
    for (int i = 0; i < array->length; i++) {
        int ret = dai_value_equal(array->elements[i], value);
        if (ret == -1) {
            DaiObjError* err =
                DaiObjError_Newf(vm, "maximum recursion depth exceeded in comparison");
            return OBJ_VAL(err);
        }
        if (ret == 1) {
            return dai_true;
        }
    }
    return dai_false;
}

static DaiValue
DaiObjArray_reversed(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc,
                     DaiValue* argv) {
    if (argc != 0) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "reversed() expected 0 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    DaiObjArray* array = AS_ARRAY(receiver);
    // copy array and reverse
    DaiObjArray* copy = DaiObjArray_pcopy(vm, array);
    DaiObjArray_preverse(copy);
    return OBJ_VAL(copy);
}

static DaiValue
DaiObjArray_reverse(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc,
                    DaiValue* argv) {
    if (argc != 0) {
        DaiObjError* err = DaiObjError_Newf(vm, "reverse() expected 0 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    DaiObjArray* array = AS_ARRAY(receiver);
    DaiObjArray_preverse(array);
    return receiver;
}

static DaiValue
DaiObjArray_sort(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err = DaiObjError_Newf(vm, "sort() expected 1 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    DaiObjArray* array = AS_ARRAY(receiver);
    DaiValue cmp       = argv[0];
    // insertion sort
    for (int i = 1; i < array->length; i++) {
        DaiValue val = array->elements[i];
        int j        = i - 1;
        for (; j >= 0; --j) {
            DaiValue ret = DaiVM_runCall(vm, cmp, 2, array->elements[j], val);
            if (IS_ERROR(ret)) {
                return ret;
            }
            if (!IS_INTEGER(ret)) {
                DaiObjError* err = DaiObjError_Newf(
                    vm, "sort cmp() expected int return value, but got %s", dai_value_ts(ret));
                return OBJ_VAL(err);
            }
            if (AS_INTEGER(ret) > 0) {
                array->elements[j + 1] = array->elements[j];   // 数据移动
            } else {
                break;
            }
        }
        array->elements[j + 1] = val;
    }
    return receiver;
}

static DaiValue
DaiObjArray_find(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err = DaiObjError_Newf(vm, "sort() expected 1 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    DaiObjArray* array = AS_ARRAY(receiver);
    DaiValue value     = argv[0];
    for (int i = 0; i < array->length; i++) {
        int ret = dai_value_equal(array->elements[i], value);
        if (ret == -1) {
            DaiObjError* err =
                DaiObjError_Newf(vm, "maximum recursion depth exceeded in comparison");
            return OBJ_VAL(err);
        }
        if (ret == 1) {
            return INTEGER_VAL(i);
        }
    }
    return INTEGER_VAL(-1);
}


enum DaiObjArrayFunctionNo {
    DaiObjArrayFunctionNo_length = 0,
    DaiObjArrayFunctionNo_append,
    DaiObjArrayFunctionNo_pop,
    DaiObjArrayFunctionNo_sub,
    DaiObjArrayFunctionNo_remove,
    DaiObjArrayFunctionNo_removeIndex,
    DaiObjArrayFunctionNo_extend,
    DaiObjArrayFunctionNo_has,
    DaiObjArrayFunctionNo_reversed,
    DaiObjArrayFunctionNo_reverse,
    DaiObjArrayFunctionNo_sort,
    DaiObjArrayFunctionNo_find,
};

static DaiObjBuiltinFunction DaiObjArrayBuiltins[] = {
    [DaiObjArrayFunctionNo_length] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "length",
            .function = &DaiObjArray_length,
        },
    [DaiObjArrayFunctionNo_append] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "append",
            .function = &DaiObjArray_append,
        },
    [DaiObjArrayFunctionNo_pop] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "pop",
            .function = &DaiObjArray_pop,
        },
    [DaiObjArrayFunctionNo_sub] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "sub",
            .function = &DaiObjArray_sub,
        },
    [DaiObjArrayFunctionNo_remove] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "remove",
            .function = &DaiObjArray_remove,
        },
    [DaiObjArrayFunctionNo_removeIndex] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "removeIndex",
            .function = &DaiObjArray_removeIndex,
        },
    [DaiObjArrayFunctionNo_extend] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "extend",
            .function = &DaiObjArray_extend,
        },
    [DaiObjArrayFunctionNo_has] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "has",
            .function = &DaiObjArray_has,
        },
    [DaiObjArrayFunctionNo_reversed] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "reversed",
            .function = &DaiObjArray_reversed,
        },
    [DaiObjArrayFunctionNo_reverse] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "reverse",
            .function = &DaiObjArray_reverse,
        },
    [DaiObjArrayFunctionNo_sort] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "sort",
            .function = &DaiObjArray_sort,
        },
    [DaiObjArrayFunctionNo_find] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "find",
            .function = &DaiObjArray_find,
        },
};

static DaiValue
DaiObjArray_get_property(DaiVM* vm, DaiValue receiver, DaiObjString* name) {
    const char* cname = name->chars;
    switch (cname[0]) {
        case 'a': {
            if (strcmp(cname, "append") == 0) {
                return OBJ_VAL(&DaiObjArrayBuiltins[DaiObjArrayFunctionNo_append]);
            }
            break;
        }
        case 'e': {
            if (strcmp(cname, "extend") == 0) {
                return OBJ_VAL(&DaiObjArrayBuiltins[DaiObjArrayFunctionNo_extend]);
            }
            break;
        }
        case 'f': {
            if (strcmp(cname, "find") == 0) {
                return OBJ_VAL(&DaiObjArrayBuiltins[DaiObjArrayFunctionNo_find]);
            }
            break;
        }
        case 'h': {
            if (strcmp(cname, "has") == 0) {
                return OBJ_VAL(&DaiObjArrayBuiltins[DaiObjArrayFunctionNo_has]);
            }
            break;
        }
        case 'l': {
            if (strcmp(cname, "length") == 0) {
                return OBJ_VAL(&DaiObjArrayBuiltins[DaiObjArrayFunctionNo_length]);
            }
            break;
        }
        case 'p': {
            if (strcmp(cname, "pop") == 0) {
                return OBJ_VAL(&DaiObjArrayBuiltins[DaiObjArrayFunctionNo_pop]);
            }
            break;
        }
        case 'r': {
            if (strcmp(cname, "remove") == 0) {
                return OBJ_VAL(&DaiObjArrayBuiltins[DaiObjArrayFunctionNo_remove]);
            }
            if (strcmp(cname, "removeIndex") == 0) {
                return OBJ_VAL(&DaiObjArrayBuiltins[DaiObjArrayFunctionNo_removeIndex]);
            }
            if (strcmp(cname, "reversed") == 0) {
                return OBJ_VAL(&DaiObjArrayBuiltins[DaiObjArrayFunctionNo_reversed]);
            }
            if (strcmp(cname, "reverse") == 0) {
                return OBJ_VAL(&DaiObjArrayBuiltins[DaiObjArrayFunctionNo_reverse]);
            }
            break;
        }
        case 's': {
            if (strcmp(cname, "sub") == 0) {
                return OBJ_VAL(&DaiObjArrayBuiltins[DaiObjArrayFunctionNo_sub]);
            }
            if (strcmp(cname, "sort") == 0) {
                return OBJ_VAL(&DaiObjArrayBuiltins[DaiObjArrayFunctionNo_sort]);
            }
            break;
        }
    }
    DaiObjError* err = DaiObjError_Newf(
        vm, "'%s' object has not property '%s'", dai_value_ts(receiver), name->chars);
    return OBJ_VAL(err);
}

static int
DaiObjArray_equal(DaiValue a, DaiValue b, int* limit) {
    DaiObjArray* array_a = AS_ARRAY(a);
    DaiObjArray* array_b = AS_ARRAY(b);
    if (array_a == array_b) {
        return true;
    }
    if (array_a->length != array_b->length) {
        return false;
    }
    for (int i = 0; i < array_a->length; i++) {
        int ret = dai_value_equal_with_limit(array_a->elements[i], array_b->elements[i], limit);
        if (ret == -1) {
            return -1;
        }
        if (ret == 0) {
            return false;
        }
    }
    return true;
}

static DaiValue
DaiObjArray_subscript_get(__attribute__((unused)) DaiVM* vm, DaiValue receiver, DaiValue index) {
    assert(IS_ARRAY(receiver));
    if (!IS_INTEGER(index)) {
        DaiObjError* err = DaiObjError_Newf(vm, "array index must be integer");
        return OBJ_VAL(err);
    }
    const DaiObjArray* array = AS_ARRAY(receiver);
    int64_t n                = AS_INTEGER(index);
    if (n < 0) {
        n += array->length;
    }
    if (n < 0 || n >= array->length) {
        DaiObjError* err = DaiObjError_Newf(vm, "array index out of range");
        return OBJ_VAL(err);
    }
    return array->elements[n];
}

static DaiValue
DaiObjArray_subscript_set(__attribute__((unused)) DaiVM* vm, DaiValue receiver, DaiValue index,
                          DaiValue value) {
    assert(IS_ARRAY(receiver));
    if (!IS_INTEGER(index)) {
        DaiObjError* err = DaiObjError_Newf(vm, "array index must be integer");
        return OBJ_VAL(err);
    }
    const DaiObjArray* array = AS_ARRAY(receiver);
    int64_t n                = AS_INTEGER(index);
    if (n < 0) {
        n += array->length;
    }
    if (n < 0 || n >= array->length) {
        DaiObjError* err = DaiObjError_Newf(vm, "array index out of range");
        return OBJ_VAL(err);
    }
    array->elements[n] = value;
    return receiver;
}

static char*
DaiObjArray_String(DaiValue value, DaiPtrArray* visited) {
    if (DaiPtrArray_contains(visited, AS_OBJ(value))) {
        return strdup("[...]");
    }
    DaiPtrArray_write(visited, AS_OBJ(value));
    DaiStringBuffer* sb = DaiStringBuffer_New();
    DaiObjArray* array  = AS_ARRAY(value);
    DaiStringBuffer_write(sb, "[");
    for (int i = 0; i < array->length; i++) {
        char* s = dai_value_string_with_visited(array->elements[i], visited);
        DaiStringBuffer_write(sb, s);
        free(s);
        if (i != array->length - 1) {
            DaiStringBuffer_write(sb, ", ");
        }
    }
    DaiStringBuffer_write(sb, "]");
    return DaiStringBuffer_getAndFree(sb, NULL);
}

static DaiValue
DaiObjArray_iter_init(__attribute__((unused)) DaiVM* vm, DaiValue receiver) {
    DaiObjArrayIterator* iterator = DaiObjArrayIterator_New(vm, AS_ARRAY(receiver));
    return OBJ_VAL(iterator);
}

static struct DaiOperation array_operation = {
    .get_property_func  = DaiObjArray_get_property,
    .set_property_func  = dai_default_set_property,
    .subscript_get_func = DaiObjArray_subscript_get,
    .subscript_set_func = DaiObjArray_subscript_set,
    .string_func        = DaiObjArray_String,
    .equal_func         = DaiObjArray_equal,
    .hash_func          = NULL,
    .iter_init_func     = DaiObjArray_iter_init,
    .iter_next_func     = NULL,
    .get_method_func    = DaiObjArray_get_property,
};

DaiObjArray*
DaiObjArray_New2(DaiVM* vm, const DaiValue* elements, const int length, const int capacity) {
    DaiObjArray* array   = ALLOCATE_OBJ(vm, DaiObjArray, DaiObjType_array);
    array->obj.operation = &array_operation;
    array->capacity      = capacity;
    array->length        = length;
    array->elements      = NULL;
    if (capacity > 0) {
        array->elements = GROW_ARRAY(DaiValue, NULL, 0, capacity);
    }
    if (elements != NULL) {
        memcpy(array->elements, elements, length * sizeof(DaiValue));
    }
    return array;
}

DaiObjArray*
DaiObjArray_New(DaiVM* vm, const DaiValue* elements, const int length) {
    return DaiObjArray_New2(vm, elements, length, length);
}


DaiObjArray*
DaiObjArray_append1(DaiVM* vm, DaiObjArray* array, int n, DaiValue* values) {
    int want = array->length + n;
    if (want > array->capacity) {
        DaiObjArray_grow(array, want);
    }
    for (int i = 0; i < n; i++) {
        array->elements[array->length + i] = values[i];
    }
    array->length += n;
    return array;
}

DaiObjArray*
DaiObjArray_append2(DaiVM* vm, DaiObjArray* array, int n, ...) {
    va_list args;
    va_start(args, n);
    int want = array->length + n;
    if (want > array->capacity) {
        DaiObjArray_grow(array, want);
    }
    for (int i = 0; i < n; i++) {
        DaiValue value                     = va_arg(args, DaiValue);
        array->elements[array->length + i] = value;
    }
    array->length += n;
    va_end(args);
    return array;
}


// #endregion

// #region DaiObjArrayIterator

static DaiValue
DaiObjArrayIterator_iter_next(__attribute__((unused)) DaiVM* vm, DaiValue receiver, DaiValue* index,
                              DaiValue* element) {
    DaiObjArrayIterator* iterator = AS_ARRAY_ITERATOR(receiver);
    if (iterator->index >= iterator->array->length) {
        return UNDEFINED_VAL;
    }
    *index   = INTEGER_VAL(iterator->index);
    *element = iterator->array->elements[iterator->index];
    iterator->index++;
    return NIL_VAL;
}

static struct DaiOperation array_iterator_operation = {
    .get_property_func  = dai_default_get_property,
    .set_property_func  = dai_default_set_property,
    .subscript_get_func = dai_default_subscript_get,
    .subscript_set_func = dai_default_subscript_set,
    .string_func        = dai_default_string_func,
    .equal_func         = dai_default_equal,
    .hash_func          = dai_default_hash,
    .iter_init_func     = dai_iter_init_return_self,
    .iter_next_func     = DaiObjArrayIterator_iter_next,
    .get_method_func    = dai_default_get_method,
};

DaiObjArrayIterator*
DaiObjArrayIterator_New(DaiVM* vm, DaiObjArray* array) {
    DaiObjArrayIterator* iterator = ALLOCATE_OBJ(vm, DaiObjArrayIterator, DaiObjType_arrayIterator);
    iterator->obj.operation       = &array_iterator_operation;
    iterator->array               = array;
    iterator->index               = 0;
    return iterator;
}

// #endregion

// #region DaiObjRangeIterator
static DaiValue
DaiObjRangeIterator_iter_next(__attribute__((unused)) DaiVM* vm, DaiValue receiver, DaiValue* index,
                              DaiValue* element) {
    DaiObjRangeIterator* iterator = AS_RANGE_ITERATOR(receiver);
    if ((iterator->step >= 0 && iterator->curr >= iterator->end) ||
        (iterator->step < 0 && iterator->curr <= iterator->end)) {
        return UNDEFINED_VAL;
    }
    *index   = INTEGER_VAL(iterator->index);
    *element = INTEGER_VAL(iterator->curr);
    iterator->curr += iterator->step;
    iterator->index++;
    return NIL_VAL;
}

static struct DaiOperation range_iterator_operation = {
    .get_property_func  = dai_default_get_property,
    .set_property_func  = dai_default_set_property,
    .subscript_get_func = dai_default_subscript_get,
    .subscript_set_func = dai_default_subscript_set,
    .string_func        = dai_default_string_func,
    .equal_func         = dai_default_equal,
    .hash_func          = dai_default_hash,
    .iter_init_func     = dai_iter_init_return_self,
    .iter_next_func     = DaiObjRangeIterator_iter_next,
    .get_method_func    = dai_default_get_method,
};

DaiObjRangeIterator*
DaiObjRangeIterator_New(DaiVM* vm, int64_t start, int64_t end, int64_t step) {
    DaiObjRangeIterator* range_iterator =
        ALLOCATE_OBJ(vm, DaiObjRangeIterator, DaiObjType_rangeIterator);
    range_iterator->obj.operation = &range_iterator_operation;
    range_iterator->start         = start;
    range_iterator->end           = end;
    range_iterator->step          = step;
    range_iterator->curr          = start;
    range_iterator->index         = 0;
    return range_iterator;
}

// #endregion

// #region 字典 DaiObjMap

typedef struct {
    DaiValue key;
    DaiValue value;
} DaiObjMapEntry;

static DaiValue
DaiObjMap_length(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 0) {
        DaiObjError* err = DaiObjError_Newf(vm, "length() expected no arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    return INTEGER_VAL(hashmap_count(AS_MAP(receiver)->map));
}

static DaiValue
DaiObjMap_get(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 1 && argc != 2) {
        DaiObjError* err = DaiObjError_Newf(vm, "get() expected 1-2 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    DaiObjMap* map = AS_MAP(receiver);
    if (!dai_value_is_hashable(argv[0])) {
        DaiObjError* err = DaiObjError_Newf(vm, "unhashable type: '%s'", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
    const DaiObjMapEntry* entry = hashmap_get(map->map, &(DaiObjMapEntry){argv[0]});
    if (entry != NULL) {
        return entry->value;
    }
    return argc == 2 ? argv[1] : NIL_VAL;
}

static DaiValue
DaiObjMap_keys(DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 0) {
        DaiObjError* err = DaiObjError_Newf(vm, "keys() expected no arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    DaiObjMap* map    = AS_MAP(receiver);
    DaiObjArray* keys = DaiObjArray_New2(vm, NULL, 0, hashmap_count(map->map));
    void* item;
    size_t iter = 0;
    while (hashmap_iter(map->map, &iter, &item)) {
        DaiObjMapEntry* entry = (DaiObjMapEntry*)item;
        DaiObjArray_append1(vm, keys, 1, &entry->key);
    }
    return OBJ_VAL(keys);
}

static DaiValue
DaiObjMap_pop(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 1 && argc != 2) {
        DaiObjError* err = DaiObjError_Newf(vm, "pop() expected 1-2 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    DaiObjMap* map = AS_MAP(receiver);
    if (!dai_value_is_hashable(argv[0])) {
        DaiObjError* err = DaiObjError_Newf(vm, "unhashable type: '%s'", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
    const DaiObjMapEntry* entry = hashmap_delete(map->map, &(DaiObjMapEntry){argv[0]});
    if (entry != NULL) {
        hashmap_delete(map->map, &(DaiObjMapEntry){argv[0]});
        return entry->value;
    }
    return argc == 2 ? argv[1] : NIL_VAL;
}

static DaiValue
DaiObjMap_has(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err = DaiObjError_Newf(vm, "has() expected 1 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    DaiObjMap* map = AS_MAP(receiver);
    if (!dai_value_is_hashable(argv[0])) {
        DaiObjError* err = DaiObjError_Newf(vm, "unhashable type: '%s'", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
    const DaiObjMapEntry* entry = hashmap_get(map->map, &(DaiObjMapEntry){argv[0]});
    return entry != NULL ? dai_true : dai_false;
}


enum DaiObjMapFunctionNo {
    DaiObjMapFunctionNo_length = 0,
    DaiObjMapFunctionNo_get,
    DaiObjMapFunctionNo_keys,
    DaiObjMapFunctionNo_pop,
    DaiObjMapFunctionNo_has,
};

static DaiObjBuiltinFunction DaiObjMapBuiltins[] = {
    [DaiObjMapFunctionNo_length] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "length",
            .function = &DaiObjMap_length,
        },
    [DaiObjMapFunctionNo_get] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "get",
            .function = &DaiObjMap_get,
        },
    [DaiObjMapFunctionNo_keys] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "keys",
            .function = &DaiObjMap_keys,
        },
    [DaiObjMapFunctionNo_pop] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "pop",
            .function = &DaiObjMap_pop,
        },
    [DaiObjMapFunctionNo_has] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "has",
            .function = &DaiObjMap_has,
        },
};

static DaiValue
DaiObjMap_get_property(DaiVM* vm, DaiValue receiver, DaiObjString* name) {
    const char* cname = name->chars;
    switch (cname[0]) {
        case 'l': {
            if (strcmp(cname, "length") == 0) {
                return OBJ_VAL(&DaiObjMapBuiltins[DaiObjMapFunctionNo_length]);
            }
            break;
        }
        case 'g': {
            if (strcmp(cname, "get") == 0) {
                return OBJ_VAL(&DaiObjMapBuiltins[DaiObjMapFunctionNo_get]);
            }
            break;
        }
        case 'k': {
            if (strcmp(cname, "keys") == 0) {
                return OBJ_VAL(&DaiObjMapBuiltins[DaiObjMapFunctionNo_keys]);
            }
            break;
        }
        case 'p': {
            if (strcmp(cname, "pop") == 0) {
                return OBJ_VAL(&DaiObjMapBuiltins[DaiObjMapFunctionNo_pop]);
            }
            break;
        }
        case 'h': {
            if (strcmp(cname, "has") == 0) {
                return OBJ_VAL(&DaiObjMapBuiltins[DaiObjMapFunctionNo_has]);
            }
            break;
        }
    }
    DaiObjError* err = DaiObjError_Newf(
        vm, "'%s' object has not property '%s'", dai_object_ts(receiver), name->chars);
    return OBJ_VAL(err);
}

static DaiValue
DaiObjMap_subscript_get(DaiVM* vm, DaiValue receiver, DaiValue index) {
    DaiObjMap* map = AS_MAP(receiver);
    if (!dai_value_is_hashable(index)) {
        DaiObjError* err = DaiObjError_Newf(vm, "unhashable type: '%s'", dai_value_ts(index));
        return OBJ_VAL(err);
    }
    const DaiObjMapEntry* entry = hashmap_get(map->map, &(DaiObjMapEntry){index});
    if (entry != NULL) {
        return entry->value;
    }
    const char* s    = dai_value_string(index);
    DaiObjError* err = DaiObjError_Newf(vm, "KeyError: %s", s);
    free((void*)s);
    return OBJ_VAL(err);
}

static DaiValue
DaiObjMap_subscript_set(DaiVM* vm, DaiValue receiver, DaiValue index, DaiValue value) {
    DaiObjMap* map = AS_MAP(receiver);
    if (!dai_value_is_hashable(index)) {
        DaiObjError* err = DaiObjError_Newf(vm, "unhashable type: '%s'", dai_value_ts(index));
        return OBJ_VAL(err);
    }
    DaiObjMapEntry entry = {index, value};
    if (hashmap_set(map->map, &entry) == NULL && hashmap_oom(map->map)) {
        DaiObjError* err = DaiObjError_Newf(vm, "Out of memory");
        return OBJ_VAL(err);
    }
    return NIL_VAL;
}

static char*
DaiObjMap_String(DaiValue value, DaiPtrArray* visited) {
    if (DaiPtrArray_contains(visited, AS_OBJ(value))) {
        return strdup("{...}");
    }
    DaiPtrArray_write(visited, AS_OBJ(value));
    DaiStringBuffer* sb = DaiStringBuffer_New();
    DaiObjMap* map      = AS_MAP(value);
    DaiStringBuffer_write(sb, "{");
    void* item;
    size_t iter = 0;
    while (hashmap_iter(map->map, &iter, &item)) {
        DaiObjMapEntry* entry = (DaiObjMapEntry*)item;
        char* key             = dai_value_string_with_visited(entry->key, visited);
        char* val             = dai_value_string_with_visited(entry->value, visited);
        DaiStringBuffer_write(sb, key);
        DaiStringBuffer_write(sb, ": ");
        DaiStringBuffer_write(sb, val);
        free(key);
        free(val);
        DaiStringBuffer_write(sb, ", ");
    }
    DaiStringBuffer_write(sb, "}");
    return DaiStringBuffer_getAndFree(sb, NULL);
}

static int
DaiObjMap_equal(DaiValue a, DaiValue b, int* limit) {
    DaiObjMap* map_a = AS_MAP(a);
    DaiObjMap* map_b = AS_MAP(b);
    if (map_a == map_b) {
        return true;
    }
    if (hashmap_count(map_a->map) != hashmap_count(map_b->map)) {
        return false;
    }
    void* item;
    size_t iter = 0;
    while (hashmap_iter(map_a->map, &iter, &item)) {
        DaiObjMapEntry* entry         = (DaiObjMapEntry*)item;
        const DaiObjMapEntry* entry_b = hashmap_get(map_b->map, entry);
        if (entry_b == NULL) {
            return false;
        }
        int ret = dai_value_equal_with_limit(entry->value, entry_b->value, limit);
        if (ret == -1) {
            return -1;
        }
        if (ret == 0) {
            return false;
        }
    }
    return true;
}

static DaiValue
DaiObjMap_iter_init(__attribute__((unused)) DaiVM* vm, DaiValue receiver) {
    DaiObjMapIterator* iterator = DaiObjMapIterator_New(vm, AS_MAP(receiver));
    return OBJ_VAL(iterator);
}

static struct DaiOperation map_operation = {
    .get_property_func  = DaiObjMap_get_property,
    .set_property_func  = dai_default_set_property,
    .subscript_get_func = DaiObjMap_subscript_get,
    .subscript_set_func = DaiObjMap_subscript_set,
    .string_func        = DaiObjMap_String,
    .equal_func         = DaiObjMap_equal,
    .hash_func          = NULL,
    .iter_init_func     = DaiObjMap_iter_init,
    .iter_next_func     = NULL,
    .get_method_func    = DaiObjMap_get_property,
};

int
DaiObjMapEntry_compare(const void* a, const void* b, void* udata) {
    const DaiObjMapEntry* entry_a = (const DaiObjMapEntry*)a;
    const DaiObjMapEntry* entry_b = (const DaiObjMapEntry*)b;
    // 因为容器不能作为键，所以 dai_value_equal 不会返回错误
    return !dai_value_equal(entry_a->key, entry_b->key);
}

uint64_t
DaiObjMapEntry_hash(const void* item, uint64_t seed0, uint64_t seed1) {
    const DaiObjMapEntry* entry = (const DaiObjMapEntry*)item;
    // 调用者负责保证 key 是可哈希的，所以这里没有错误处理
    return dai_value_hash(entry->key, seed0, seed1);
}

DaiObjError*
DaiObjMap_New(DaiVM* vm, const DaiValue* values, int length, DaiObjMap** map_ret) {
    DaiObjMap* map     = ALLOCATE_OBJ(vm, DaiObjMap, DaiObjType_map);
    map->obj.operation = &map_operation;
    map->map           = hashmap_new(sizeof(DaiObjMapEntry),
                           length,
                           0,
                           0,
                           DaiObjMapEntry_hash,
                           DaiObjMapEntry_compare,
                           NULL,
                           vm);
    if (map->map == NULL) {
        return DaiObjError_Newf(vm, "DaiObjMap_New: Out of memory");
    }
    struct hashmap* h = map->map;
    for (int i = 0; i < length; i++) {
        if (!dai_value_is_hashable(values[i * 2])) {
            return DaiObjError_Newf(vm, "unhashable type: '%s'", dai_value_ts(values[i * 2]));
        }
        DaiObjMapEntry entry = {values[i * 2], values[i * 2 + 1]};
        if (hashmap_set(h, &entry) == NULL && hashmap_oom(h)) {
            return DaiObjError_Newf(vm, "Out of memory");
        }
    }
    *map_ret = map;
    return NULL;
}

void
DaiObjMap_Free(DaiVM* vm, DaiObjMap* map) {
    hashmap_free(map->map);
    VM_FREE(vm, DaiObjMap, map);
}

bool
DaiObjMap_iter(DaiObjMap* map, size_t* i, DaiValue* key, DaiValue* value) {
    void* item;
    if (hashmap_iter(map->map, i, &item)) {
        DaiObjMapEntry entry = *(DaiObjMapEntry*)item;
        *key                 = entry.key;
        *value               = entry.value;
        return true;
    }
    return false;
}

void
DaiObjMap_cset(DaiObjMap* map, DaiValue key, DaiValue value) {
    DaiObjMapEntry entry = {key, value};
    const void* res      = hashmap_set(map->map, &entry);
    if (res == NULL && hashmap_oom(map->map)) {
        dai_error("DaiObjMap_cset: Out of memory\n");
        exit(1);
    }
}

bool
DaiObjMap_cget(DaiObjMap* map, DaiValue key, DaiValue* value) {
    const DaiObjMapEntry* entry = hashmap_get(map->map, &(DaiObjMapEntry){key});
    if (entry != NULL) {
        *value = entry->value;
        return true;
    }
    return false;
}
// #endregion

// #region DaiObjMapIterator

static DaiValue
DaiObjMapIterator_iter_next(__attribute__((unused)) DaiVM* vm, DaiValue receiver, DaiValue* index,
                            DaiValue* element) {
    DaiObjMapIterator* iterator = AS_MAP_ITERATOR(receiver);
    DaiObjMap* map              = iterator->map;
    void* item;
    if (hashmap_iter(map->map, &iterator->map_index, &item)) {
        DaiObjMapEntry* entry = (DaiObjMapEntry*)item;
        *index                = entry->key;
        *element              = entry->value;
        return NIL_VAL;
    }
    return UNDEFINED_VAL;
}

static struct DaiOperation map_iterator_operation = {
    .get_property_func  = dai_default_get_property,
    .set_property_func  = dai_default_set_property,
    .subscript_get_func = dai_default_subscript_get,
    .subscript_set_func = dai_default_subscript_set,
    .string_func        = dai_default_string_func,
    .equal_func         = dai_default_equal,
    .hash_func          = dai_default_hash,
    .iter_init_func     = dai_iter_init_return_self,
    .iter_next_func     = DaiObjMapIterator_iter_next,
    .get_method_func    = dai_default_get_method,
};

DaiObjMapIterator*
DaiObjMapIterator_New(DaiVM* vm, DaiObjMap* map) {
    DaiObjMapIterator* iterator = ALLOCATE_OBJ(vm, DaiObjMapIterator, DaiObjType_mapIterator);
    iterator->obj.operation     = &map_iterator_operation;
    iterator->map               = map;
    iterator->map_index         = 0;
    return iterator;
}

// #endregion

// #region 错误

static char*
DaiObjError_String(DaiValue value, __attribute__((unused)) DaiPtrArray* visited) {
    int size  = strlen(AS_ERROR(value)->message) + 16;
    char* buf = ALLOCATE(char, size);
    snprintf(buf, size, "Error: %s", AS_ERROR(value)->message);
    return buf;
}

static uint64_t
DaiObjError_hash(DaiValue value) {
    return hash_string(AS_ERROR(value)->message, strlen(AS_ERROR(value)->message));
}

static struct DaiOperation error_operation = {
    .get_property_func  = dai_default_get_property,
    .set_property_func  = dai_default_set_property,
    .subscript_get_func = dai_default_subscript_get,
    .subscript_set_func = dai_default_subscript_set,
    .string_func        = DaiObjError_String,
    .equal_func         = dai_default_equal,
    .hash_func          = DaiObjError_hash,
    .iter_init_func     = NULL,
    .iter_next_func     = NULL,
    .get_method_func    = dai_default_get_method,
};

DaiObjError*
DaiObjError_Newf(DaiVM* vm, const char* format, ...) {
    DaiObjError* error   = ALLOCATE_OBJ(vm, DaiObjError, DaiObjType_error);
    error->obj.operation = &error_operation;
    va_list args;
    va_start(args, format);
    vsnprintf(error->message, sizeof(error->message), format, args);
    va_end(args);
    return error;
}

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

static struct DaiOperation c_function_operation = {
    .get_property_func  = dai_default_get_property,
    .set_property_func  = dai_default_set_property,
    .subscript_get_func = dai_default_subscript_get,
    .subscript_set_func = dai_default_subscript_set,
    .string_func        = DaiObjCFunction_String,
    .equal_func         = dai_default_equal,
    .hash_func          = dai_default_hash,
    .iter_init_func     = NULL,
    .iter_next_func     = NULL,
    .get_method_func    = dai_default_get_method,
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

const char*
dai_object_ts(DaiValue value) {
    switch (OBJ_TYPE(value)) {
        case DaiObjType_boundMethod: {
            return "bound_method";
        }
        case DaiObjType_instance: {
            return AS_INSTANCE(value)->klass->name->chars;
        }
        case DaiObjType_class: {
            return "class";
        }
        case DaiObjType_function: return "function";
        case DaiObjType_string: return "string";
        case DaiObjType_builtinFn: return "builtin-function";
        case DaiObjType_closure: {
            return "function";
        }
        case DaiObjType_array: return "array";
        case DaiObjType_arrayIterator: return "array_iterator";
        case DaiObjType_map: return "map";
        case DaiObjType_mapIterator: return "map_iterator";
        case DaiObjType_rangeIterator: return "range_iterator";
        case DaiObjType_error: return "error";
        case DaiObjType_cFunction: return "c-function";
        case DaiObjType_module: return "module";
        case DaiObjType_tuple: return "tuple";
    }
    return "unknown";
}
