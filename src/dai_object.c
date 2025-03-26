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
};

static char*
DaiObjBuiltinFunction_String(DaiValue value, __attribute__((unused)) DaiPtrArray* visited) {
    DaiObjBuiltinFunction* builtin = AS_BUILTINFN(value);
    size_t size                    = strlen(builtin->name) + 16;
    char* buf                      = ALLOCATE(char, size);
    snprintf(buf, size, "<builtin-fn %s>", builtin->name);
    return buf;
}

static struct DaiOperation builtin_function_operation = {
    .get_property_func  = dai_default_get_property,
    .set_property_func  = dai_default_set_property,
    .subscript_get_func = dai_default_subscript_get,
    .subscript_set_func = dai_default_subscript_set,
    .string_func        = DaiObjBuiltinFunction_String,
    .equal_func         = dai_default_equal,
    .hash_func          = dai_default_hash,
    .iter_init_func     = NULL,
    .iter_next_func     = NULL,
};

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

// note: name 和 filename 要在堆上分配，同时转移所有权
DaiObjModule*
DaiObjModule_New(DaiVM* vm, const char* name, const char* filename) {
    assert(name != NULL && filename != NULL);
    DaiObjModule* module = ALLOCATE_OBJ(vm, DaiObjModule, DaiObjType_module);
    module->name         = dai_take_string_intern(vm, (char*)name, strlen(name));
    module->filename     = dai_take_string_intern(vm, (char*)filename, strlen(filename));
    DaiChunk_init(&module->chunk, filename);
    module->globalSymbolTable = DaiSymbolTable_New();
    module->globals           = malloc(sizeof(DaiValue) * BUILTIN_GLOBALS_COUNT);
    module->globalInitCount   = BUILTIN_GLOBALS_COUNT;
    module->globalCapacity    = BUILTIN_GLOBALS_COUNT;
    DaiSymbolTable_setOuter(module->globalSymbolTable, vm->builtinSymbolTable);
    module->max_local_count = 0;

    // 设置两个内置的全局变量 __name__ 和 __file__
    DaiSymbol symbol;
    symbol = DaiSymbolTable_define(module->globalSymbolTable, "__name__", true);
    assert(symbol.index == 0);
    symbol = DaiSymbolTable_define(module->globalSymbolTable, "__file__", true);
    assert(symbol.index == 1);
    module->globals[0] = OBJ_VAL(module->name);
    module->globals[1] = OBJ_VAL(module->filename);
    return module;
}

void
DaiObjModule_afterCompile(DaiObjModule* module) {
    module->compiled = true;
    // 按实际的全局变量数量重新分配内存
    int count = DaiSymbolTable_count(module->globalSymbolTable);
    assert(count >= BUILTIN_GLOBALS_COUNT);
    module->globals = realloc(module->globals, sizeof(DaiValue) * count);
    if (module->globals == NULL) {
        dai_error("realloc globals(%zu bytes) error\n", count * sizeof(DaiValue));
        abort();
    }
    for (int i = module->globalInitCount; i < count; i++) {
        module->globals[i] = UNDEFINED_VAL;
    }
}

bool
DaiObjModule_getGlobal(DaiObjModule* module, const char* name, DaiValue* value) {
    assert(module->compiled);
    DaiSymbol symbol;

    if (!DaiSymbolTable_resolve(module->globalSymbolTable, name, &symbol)) {
        return false;
    }
    *value = module->globals[symbol.index];
    return true;
}

bool
DaiObjModule_setGlobal(DaiObjModule* module, const char* name, DaiValue value) {
    assert(module->compiled);
    DaiSymbol symbol;
    if (!DaiSymbolTable_resolve(module->globalSymbolTable, name, &symbol)) {
        return false;
    }
    module->globals[symbol.index] = value;
    return true;
}

bool
DaiObjModule_addGlobal(DaiObjModule* module, const char* name, DaiValue value) {
    assert(!module->compiled);
    DaiSymbol symbol;
    if (DaiSymbolTable_resolve(module->globalSymbolTable, name, &symbol)) {
        return false;
    }
    symbol = DaiSymbolTable_define(module->globalSymbolTable, name, true);
    if (symbol.index + 1 > module->globalCapacity) {
        module->globalCapacity = GROW_CAPACITY(module->globalCapacity);
        module->globals = realloc(module->globals, sizeof(DaiValue) * module->globalCapacity);
        for (int i = module->globalInitCount; i < module->globalCapacity; i++) {
            module->globals[i] = UNDEFINED_VAL;
        }
    }
    assert(symbol.index == module->globalInitCount);
    module->globals[symbol.index] = value;
    module->globalInitCount++;
    return true;
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

// #region 类与实例

static bool
DaiObjInstance_get_method(DaiVM* vm, DaiObjClass* klass, DaiObjString* name, DaiValue* method) {
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
    DaiValue value;
    if (DaiTable_get(&instance->fields, name, &value)) {
        return value;
    }
    if (DaiObjInstance_get_method(vm, instance->klass, name, &value)) {
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
    if (!DaiTable_set_if_exist(&instance->fields, name, value)) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "'%s' object has not property '%s'", dai_object_ts(receiver), name->chars);
        return OBJ_VAL(err);
    }
    return NIL_VAL;
}

static bool
DaiObjClass_get_method(DaiVM* vm, DaiObjClass* klass, DaiObjString* name, DaiValue* method) {
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
    DaiValue value;
    if (DaiTable_get(&klass->class_fields, name, &value)) {
        return value;
    }
    if (DaiObjClass_get_method(vm, klass, name, &value)) {
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
    if (!DaiTable_set_if_exist(&klass->class_fields, name, value)) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "'%s' object has not property '%s'", dai_object_ts(receiver), name->chars);
        return OBJ_VAL(err);
    }
    return NIL_VAL;
}

static char*
DaiObjClass_String(DaiValue value, __attribute__((unused)) DaiPtrArray* visited) {
    DaiObjClass* klass = AS_CLASS(value);
    int size           = klass->name->length + 16;
    char* buf          = ALLOCATE(char, size);
    snprintf(buf, size, "<class %s>", klass->name->chars);
    return buf;
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
};

DaiObjClass*
DaiObjClass_New(DaiVM* vm, DaiObjString* name) {
    DaiObjClass* klass   = ALLOCATE_OBJ(vm, DaiObjClass, DaiObjType_class);
    klass->obj.operation = &class_operation;
    klass->name          = name;
    DaiTable_init(&klass->class_fields);
    DaiTable_init(&klass->class_methods);
    DaiTable_init(&klass->methods);
    DaiTable_init(&klass->fields);
    DaiValueArray_init(&klass->field_names);
    klass->parent = NULL;
    // klass->init   = OBJ_VAL(&builtin_init);
    klass->init = NIL_VAL;
    return klass;
}

DaiValue
DaiObjClass_call(DaiObjClass* klass, DaiVM* vm, int argc, DaiValue* argv) {
    DaiObjInstance* instance = DaiObjInstance_New(vm, klass);
    // 设置此次调用的实例
    vm->stack_top[-argc - 1]   = OBJ_VAL(instance);
    DaiValueArray* field_names = &(instance->klass->field_names);
    if (argc > field_names->count) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "Too many arguments, max expected=%d got=%d", field_names->count, argc);
        return OBJ_VAL(err);
    }
    if (IS_NIL(instance->klass->init)) {
        for (int i = 0; i < argc; i++) {
            DaiObjString* name = AS_STRING(field_names->values[i]);
            DaiTable_set(&instance->fields, name, argv[i]);
        }
    } else {
        DaiValue res = DaiVM_runCall2(vm, instance->klass->init, argc);
        if (IS_ERROR(res)) {
            return res;
        }
    }
    // check all fields are initialized
    for (int i = argc; i < field_names->count; i++) {
        DaiObjString* name = AS_STRING(field_names->values[i]);
        DaiValue value     = UNDEFINED_VAL;
        DaiTable_get(&instance->fields, name, &value);
        if (IS_UNDEFINED(value)) {
            DaiObjError* err = DaiObjError_Newf(vm,
                                                "'%s' object has uninitialized field '%s'",
                                                dai_object_ts(OBJ_VAL(instance)),
                                                name->chars);
            return OBJ_VAL(err);
        }
    }
    return OBJ_VAL(instance);
}

static char*
DaiObjInstance_String(DaiValue value, __attribute__((unused)) DaiPtrArray* visited) {
    DaiObjInstance* instance = AS_INSTANCE(value);
    int size                 = instance->klass->name->length + 64;
    char* buf                = ALLOCATE(char, size);
    snprintf(buf, size, "<instance of %s at %p>", instance->klass->name->chars, instance);
    return buf;
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
};


DaiObjInstance*
DaiObjInstance_New(DaiVM* vm, DaiObjClass* klass) {
    DaiObjInstance* instance = ALLOCATE_OBJ(vm, DaiObjInstance, DaiObjType_instance);
    instance->obj.operation  = &instance_operation;
    instance->klass          = klass;
    DaiTable_init(&instance->fields);
    DaiTable_copy(&instance->klass->fields, &instance->fields);
    return instance;
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
        if (DaiObjClass_get_method(vm, klass, name, &method)) {
            return DaiObjBoundMethod_New(vm, receiver, method);
        }
    } else {
        if (DaiObjInstance_get_method(vm, klass, name, &method)) {
            return DaiObjBoundMethod_New(vm, receiver, method);
        }
    }
    return NULL;
}
void
DaiObj_get_method(DaiVM* vm, DaiObjClass* klass, DaiValue receiver, DaiObjString* name,
                  DaiValue* method) {
    if (IS_CLASS(receiver)) {
        if (DaiObjClass_get_method(vm, klass, name, method)) {
            return;
        }
    } else {
        if (DaiObjInstance_get_method(vm, klass, name, method)) {
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
    struct hashmap* h  = map->map;
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
    }
    return "unknown";
}

// #region 内置函数
static DaiValue
builtin_print(__attribute__((unused)) DaiVM* vm, __attribute__((unused)) DaiValue receiver,
              int argc, DaiValue* argv) {
    for (int i = 0; i < argc; i++) {
        dai_print_value(argv[i]);
        dai_log(" ");
    }
    dai_log("\n");
    return NIL_VAL;
}

static DaiValue
builtin_len(__attribute__((unused)) DaiVM* vm, __attribute__((unused)) DaiValue receiver, int argc,
            DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err = DaiObjError_Newf(vm, "len() expected 1 argument, but got %d", argc);
        return OBJ_VAL(err);
    }
    const DaiValue arg = argv[0];
    if (IS_STRING(arg)) {
        return INTEGER_VAL(AS_STRING(arg)->length);
    } else if (IS_ARRAY(arg)) {
        return INTEGER_VAL(AS_ARRAY(arg)->length);
    } else {
        DaiObjError* err = DaiObjError_Newf(vm, "'len' not supported '%s'", dai_value_ts(arg));
        return OBJ_VAL(err);
    }
    return NIL_VAL;
}

static DaiValue
builtin_type(__attribute__((unused)) DaiVM* vm, __attribute__((unused)) DaiValue receiver, int argc,
             DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err = DaiObjError_Newf(vm, "type() expected 1 argument, but got %d", argc);
        return OBJ_VAL(err);
    }
    const DaiValue arg = argv[0];
    const char* s      = dai_value_ts(arg);
    return OBJ_VAL(dai_copy_string_intern(vm, s, strlen(s)));
}

static DaiValue
builtin_assert(__attribute__((unused)) DaiVM* vm, __attribute__((unused)) DaiValue receiver,
               int argc, DaiValue* argv) {
    if ((argc != 1) && (argc != 2)) {
        DaiObjError* err = DaiObjError_Newf(vm, "assert() expected 2 argument, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (argc == 2 && !IS_STRING(argv[1])) {
        DaiObjError* err = DaiObjError_Newf(vm, "assert() expected string as second argument");
        return OBJ_VAL(err);
    }
    if (!dai_value_is_truthy(argv[0])) {
        if (argc == 1) {
            DaiObjError* err = DaiObjError_Newf(vm, "assertion failed");
            return OBJ_VAL(err);
        }
        DaiObjError* err = DaiObjError_Newf(vm, "assertion failed: %s", AS_STRING(argv[1])->chars);
        return OBJ_VAL(err);
    }
    return NIL_VAL;
}

static DaiValue
builtin_assert_eq(__attribute__((unused)) DaiVM* vm, __attribute__((unused)) DaiValue receiver,
                  int argc, DaiValue* argv) {
    if ((argc != 2) && (argc != 3)) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "assert_eq() expected 2 or 3 argument, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (argc == 3 && !IS_STRING(argv[2])) {
        DaiObjError* err = DaiObjError_Newf(vm, "assert_eq() expected string as third argument");
        return OBJ_VAL(err);
    }
    if (!dai_value_equal(argv[0], argv[1])) {
        char* s1         = dai_value_string(argv[0]);
        char* s2         = dai_value_string(argv[1]);
        DaiObjError* err = NULL;
        if (argc == 2) {
            err = DaiObjError_Newf(vm, "assertion failed: %s != %s", s1, s2);
        } else {
            err = DaiObjError_Newf(
                vm, "assertion failed: %s != %s %s", s1, s2, AS_STRING(argv[2])->chars);
        }
        free(s1);
        free(s2);
        return OBJ_VAL(err);
    }
    return NIL_VAL;
}

static DaiValue
builtin_time_time(__attribute__((unused)) DaiVM* vm, __attribute__((unused)) DaiValue receiver,
                  int argc, DaiValue* argv) {
    if (argc != 0) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "time_time() expected no arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return INTEGER_VAL(ts.tv_sec);
}

static DaiValue
builtin_time_timef(__attribute__((unused)) DaiVM* vm, __attribute__((unused)) DaiValue receiver,
                   int argc, DaiValue* argv) {
    if (argc != 0) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "time_timef() expected no arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return FLOAT_VAL(ts.tv_sec + (double)ts.tv_nsec / 1e9);
}


static DaiValue
builtin_time_sleep(__attribute__((unused)) DaiVM* vm, __attribute__((unused)) DaiValue receiver,
                   int argc, DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "time_sleep() expected 1 argument, but got %d", argc);
        return OBJ_VAL(err);
    }
    DaiValue val = argv[0];
    struct timespec req;
    if (IS_INTEGER(val)) {
        req.tv_sec  = AS_INTEGER(val);
        req.tv_nsec = 0;
    } else if (IS_FLOAT(val)) {
        req.tv_sec  = (time_t)AS_FLOAT(val);
        req.tv_nsec = (long)((AS_FLOAT(val) - (double)req.tv_sec) * 1e9);
    } else {
        DaiObjError* err = DaiObjError_Newf(
            vm, "time_sleep() expected number arguments, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
    nanosleep(&req, NULL);
    return NIL_VAL;
}

static DaiValue
builtin_range(__attribute__((unused)) DaiVM* vm, __attribute__((unused)) DaiValue receiver,
              int argc, DaiValue* argv) {
    if (argc == 0 || argc > 3) {
        DaiObjError* err = DaiObjError_Newf(vm, "range() expected 1-3 argument, but got %d", argc);
        return OBJ_VAL(err);
    }
    for (int i = 0; i < argc; ++i) {
        if (!IS_INTEGER(argv[i])) {
            DaiObjError* err = DaiObjError_Newf(
                vm, "range() expected int arguments, but got %s", dai_value_ts(argv[0]));
            return OBJ_VAL(err);
        }
    }
    int64_t start = 0, end = 0, step = 1;
    if (argc == 1) {
        end = AS_INTEGER(argv[0]);
    } else if (argc == 2) {
        start = AS_INTEGER(argv[0]);
        end   = AS_INTEGER(argv[1]);
    } else {
        start = AS_INTEGER(argv[0]);
        end   = AS_INTEGER(argv[1]);
        step  = AS_INTEGER(argv[2]);
    }
    DaiObjRangeIterator* iterator = DaiObjRangeIterator_New(vm, start, end, step);
    return OBJ_VAL(iterator);
}

static DaiValue
builtin_math_sqrt(__attribute__((unused)) DaiVM* vm, __attribute__((unused)) DaiValue receiver,
                  int argc, DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "math_sqrt() expected 1 argument, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (IS_INTEGER(argv[0])) {
        double n = sqrt(AS_INTEGER(argv[0]));
        return FLOAT_VAL(n);
    } else if (IS_FLOAT(argv[0])) {
        double n = sqrt(AS_FLOAT(argv[0]));
        return FLOAT_VAL(n);
    } else {
        DaiObjError* err = DaiObjError_Newf(
            vm, "math_sqrt() expected number arguments, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
}

static DaiValue
builtin_math_sin(__attribute__((unused)) DaiVM* vm, __attribute__((unused)) DaiValue receiver,
                 int argc, DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err = DaiObjError_Newf(vm, "math_sin() expected 1 argument, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (IS_INTEGER(argv[0])) {
        double n = sin(AS_INTEGER(argv[0]));
        return FLOAT_VAL(n);
    } else if (IS_FLOAT(argv[0])) {
        double n = sin(AS_FLOAT(argv[0]));
        return FLOAT_VAL(n);
    } else {
        DaiObjError* err = DaiObjError_Newf(
            vm, "math_sin() expected number arguments, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
}

static DaiValue
builtin_math_cos(__attribute__((unused)) DaiVM* vm, __attribute__((unused)) DaiValue receiver,
                 int argc, DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err = DaiObjError_Newf(vm, "math_cos() expected 1 argument, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (IS_INTEGER(argv[0])) {
        double n = cos(AS_INTEGER(argv[0]));
        return FLOAT_VAL(n);
    } else if (IS_FLOAT(argv[0])) {
        double n = cos(AS_FLOAT(argv[0]));
        return FLOAT_VAL(n);
    } else {
        DaiObjError* err = DaiObjError_Newf(
            vm, "math_cos() expected number arguments, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
}

static DaiValue
builtin_abs(__attribute__((unused)) DaiVM* vm, __attribute__((unused)) DaiValue receiver, int argc,
            DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err = DaiObjError_Newf(vm, "abs() expected 1 argument, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (IS_INTEGER(argv[0])) {
        return INTEGER_VAL(llabs(AS_INTEGER(argv[0])));
    } else if (IS_FLOAT(argv[0])) {
        return FLOAT_VAL(fabs(AS_FLOAT(argv[0])));
    } else {
        DaiObjError* err = DaiObjError_Newf(
            vm, "abs() expected number arguments, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
}

DaiObjBuiltinFunction builtin_funcs[BUILTIN_FUNCTION_COUNT] = {
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "print",
        .function = builtin_print,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "len",
        .function = builtin_len,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "type",
        .function = builtin_type,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "assert",
        .function = builtin_assert,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "assert_eq",
        .function = builtin_assert_eq,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "time_time",
        .function = builtin_time_time,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "time_timef",
        .function = builtin_time_timef,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "time_sleep",
        .function = builtin_time_sleep,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "range",
        .function = builtin_range,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "math_sqrt",
        .function = builtin_math_sqrt,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "math_sin",
        .function = builtin_math_sin,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "math_cos",
        .function = builtin_math_cos,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "abs",
        .function = builtin_abs,
    },

    {
        .name = NULL,
    },
};

// #endregion
