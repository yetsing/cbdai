#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dai_memory.h"
#include "dai_object.h"
#include "dai_table.h"
#include "dai_value.h"
#include "dai_vm.h"

#define ALLOCATE_OBJ(vm, type, objectType) (type*)allocate_object(vm, sizeof(type), objectType)

static DaiValue
dai_default_get_property(DaiVM* vm, DaiValue receiver, DaiObjString* name) {
    dai_error(
        "PropertyError: '%s' object has no property '%s'\n", dai_value_ts(receiver), name->chars);
    assert(false);
}

static DaiValue
dai_default_set_property(DaiVM* vm, DaiValue receiver, DaiObjString* name, DaiValue value) {
    dai_error(
        "PropertyError: '%s' object has no property '%s'\n", dai_value_ts(receiver), name->chars);
    assert(false);
}

static DaiValue
DaiObjArray_subscriptGet(__attribute__((unused)) DaiVM* vm, DaiValue receiver, DaiValue index);
static DaiValue
DaiObjArray_subscriptSet(__attribute__((unused)) DaiVM* vm, DaiValue receiver, DaiValue index,
                         DaiValue value);
static DaiValue
builtin_subscript_get_fn(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc,
                         DaiValue* argv) {
    if (!IS_OBJ(receiver)) {
        dai_error("'%s' object is not subscriptable\n", dai_value_ts(receiver));
        assert(false);
    }
    const DaiObj* obj    = AS_OBJ(receiver);
    const DaiValue index = argv[0];
    switch (obj->type) {
        case DaiObjType_array: {
            return DaiObjArray_subscriptGet(vm, receiver, index);
            break;
        }
        default: {
            dai_error("'%s' object is not subscriptable\n", dai_value_ts(receiver));
            assert(false);
        }
    }
    return UNDEFINED_VAL;
}
static DaiValue
builtin_subscript_set_fn(__attribute__((unused)) DaiVM* vm,
                         __attribute__((unused)) DaiValue receiver, int argc, DaiValue* argv) {
    if (!IS_OBJ(receiver)) {
        dai_error("'%s' object is not subscriptable\n", dai_value_ts(receiver));
        assert(false);
    }
    // argv 排列为 value, object, index
    const DaiObj* obj    = AS_OBJ(receiver);
    const DaiValue value = argv[0];
    const DaiValue index = argv[2];
    switch (obj->type) {
        case DaiObjType_array: {
            return DaiObjArray_subscriptSet(vm, receiver, index, value);
            break;
        }
        default: {
            dai_error("'%s' object is not subscriptable\n", dai_value_ts(receiver));
            assert(false);
        }
    }
    return UNDEFINED_VAL;
}

DaiObjBuiltinFunction builtin_subscript_get = {
    {.type = DaiObjType_builtinFn},
    .name     = "subscript_get",
    .function = builtin_subscript_get_fn,
};

DaiObjBuiltinFunction builtin_subscript_set = {
    {.type = DaiObjType_builtinFn},
    .name     = "subscript_set",
    .function = builtin_subscript_set_fn,
};

static DaiObj*
allocate_object(DaiVM* vm, size_t size, DaiObjType type) {
    DaiObj* object            = (DaiObj*)vm_reallocate(vm, NULL, 0, size);
    object->type              = type;
    object->is_marked         = false;
    object->next              = vm->objects;
    object->get_property_func = dai_default_get_property;
    object->set_property_func = dai_default_set_property;
    vm->objects               = object;
#ifdef DEBUG_LOG_GC
    dai_log("%p allocate %zu for %d\n", (void*)object, size, type);

#endif

    return object;
}

DaiObjFunction*
DaiObjFunction_New(DaiVM* vm, const char* name) {
    DaiObjFunction* function = ALLOCATE_OBJ(vm, DaiObjFunction, DaiObjType_function);
    function->arity          = 0;
    function->name           = dai_copy_string(vm, name, strlen(name));
    DaiChunk_init(&function->chunk, function->name->chars);
    function->superclass = NULL;
    return function;
}

const char*
DaiObjFunction_name(DaiObjFunction* function) {
    return function->name->chars;
}

DaiObjClosure*
DaiObjClosure_New(DaiVM* vm, DaiObjFunction* function) {
    DaiObjClosure* closure = ALLOCATE_OBJ(vm, DaiObjClosure, DaiObjType_closure);
    closure->function      = function;
    closure->frees         = NULL;
    closure->free_count    = 0;
    return closure;
}

const char*
DaiObjClosure_name(DaiObjClosure* closure) {
    return closure->function->name->chars;
}

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

    // todo Exception 将错误也作为一种值返回
    dai_error(
        "PropertyError: '%s' object has no property '%s'", dai_object_ts(receiver), name->chars);
    assert(false);
    return UNDEFINED_VAL;
};
static DaiValue
DaiObjInstance_set_property(DaiVM* vm, DaiValue receiver, DaiObjString* name, DaiValue value) {
    assert(IS_OBJ(receiver));
    DaiObjInstance* instance = AS_INSTANCE(receiver);
    if (!DaiTable_set_if_exist(&instance->fields, name, value)) {
        // todo Exception 将错误也作为一种值返回
        dai_error("PropertyError: '%s' object has no property '%s'",
                  dai_object_ts(receiver),
                  name->chars);
        assert(false);
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

    // todo Exception 将错误也作为一种值返回
    dai_error(
        "PropertyError: '%s' object has no property '%s'", dai_object_ts(receiver), name->chars);
    assert(false);
    return UNDEFINED_VAL;
};
static DaiValue
DaiObjClass_set_property(DaiVM* vm, DaiValue receiver, DaiObjString* name, DaiValue value) {
    assert(IS_OBJ(receiver));
    DaiObjClass* klass = AS_CLASS(receiver);
    if (!DaiTable_set_if_exist(&klass->class_fields, name, value)) {
        // todo Exception 将错误也作为一种值返回
        dai_error("PropertyError: '%s' object has no property '%s'",
                  dai_object_ts(receiver),
                  name->chars);
        assert(false);
    }
    return NIL_VAL;
}

DaiObjClass*
DaiObjClass_New(DaiVM* vm, DaiObjString* name) {
    DaiObjClass* klass           = ALLOCATE_OBJ(vm, DaiObjClass, DaiObjType_class);
    klass->obj.get_property_func = DaiObjClass_get_property;
    klass->obj.set_property_func = DaiObjClass_set_property;
    klass->name                  = name;
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
        // todo Exception 将错误也作为一种值返回
        dai_error("Too many arguments, max expected=%d got=%d.\n", field_names->count, argc);
        assert(false);
    }
    if (IS_NIL(instance->klass->init)) {
        for (int i = 0; i < argc; i++) {
            DaiObjString* name = AS_STRING(field_names->values[i]);
            DaiTable_set(&instance->fields, name, argv[i]);
        }
    } else {
        // todo check return error
        DaiVM_runCall2(vm, instance->klass->init, argc);
    }
    // check all fields are initialized
    for (int i = argc; i < field_names->count; i++) {
        DaiObjString* name = AS_STRING(field_names->values[i]);
        DaiValue value     = UNDEFINED_VAL;
        DaiTable_get(&instance->fields, name, &value);
        if (IS_UNDEFINED(value)) {
            dai_error("'%s' object has uninitialized field '%s'\n",
                      dai_object_ts(OBJ_VAL(instance)),
                      name->chars);
            assert(false);
        }
    }
    return OBJ_VAL(instance);
}


DaiObjInstance*
DaiObjInstance_New(DaiVM* vm, DaiObjClass* klass) {
    DaiObjInstance* instance        = ALLOCATE_OBJ(vm, DaiObjInstance, DaiObjType_instance);
    instance->obj.get_property_func = DaiObjInstance_get_property;
    instance->obj.set_property_func = DaiObjInstance_set_property;
    instance->klass                 = klass;
    DaiTable_init(&instance->fields);
    DaiTable_copy(&instance->klass->fields, &instance->fields);
    return instance;
}

DaiObjBoundMethod*
DaiObjBoundMethod_New(DaiVM* vm, DaiValue receiver, DaiValue method) {
    DaiObjBoundMethod* bound_method = ALLOCATE_OBJ(vm, DaiObjBoundMethod, DaiObjType_boundMethod);
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

    // todo Exception 将错误也作为一种值返回
    dai_error("PropertyError: 'super' object has no property '%s'", name->chars);
    assert(false);
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
    // todo Exception 将错误也作为一种值返回
    dai_error("PropertyError: 'super' object has no property '%s'", name->chars);
    assert(false);
    return;
}
// #endregion

// #region 字符串
static DaiObjString*
allocate_string(DaiVM* vm, char* chars, int length, uint32_t hash) {
    DaiObjString* string = ALLOCATE_OBJ(vm, DaiObjString, DaiObjType_string);
    string->length       = length;
    string->chars        = chars;
    string->hash         = hash;
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
DaiObjString*
dai_take_string(DaiVM* vm, char* chars, int length) {
    uint32_t hash          = hash_string(chars, length);
    DaiObjString* interned = DaiTable_findString(&vm->strings, chars, length, hash);
    if (interned != NULL) {
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }

    return allocate_string(vm, chars, length, hash);
}
DaiObjString*
dai_copy_string(DaiVM* vm, const char* chars, int length) {
    uint32_t hash          = hash_string(chars, length);
    DaiObjString* interned = DaiTable_findString(&vm->strings, chars, length, hash);
    if (interned != NULL) return interned;

    char* heap_chars = VM_ALLOCATE(vm, char, length + 1);
    memcpy(heap_chars, chars, length);
    heap_chars[length] = '\0';
    return allocate_string(vm, heap_chars, length, hash);
}
// #endregion

// #region 数组 DaiObjArray
// TODO 缩容

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
        dai_error("TypeError: length() expected no arguments, but got %d\n", argc);
        assert(false);
    }
    return INTEGER_VAL(AS_ARRAY(receiver)->length);
}

static DaiValue
DaiObjArray_add(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc == 0) {
        dai_error("TypeError: add() expected one or more arguments, but got no arguments\n");
        assert(false);
    }
    DaiObjArray* array = AS_ARRAY(receiver);
    int want           = array->length + argc;
    if (want > array->capacity) {
        int old_capacity = array->capacity;
        array->capacity  = GROW_CAPACITY(array->capacity);
        if (want > array->capacity) {
            array->capacity = want;
        }
        array->elements = GROW_ARRAY(DaiValue, array->elements, old_capacity, array->capacity);
    }
    for (int i = 0; i < argc; i++) {
        array->elements[array->length + i] = argv[i];
    }
    array->length += argc;
    return receiver;
}

static DaiValue
DaiObjArray_pop(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 0) {
        dai_error("TypeError: pop() expected no arguments, but got %d\n", argc);
        assert(false);
    }
    DaiObjArray* array = AS_ARRAY(receiver);
    if (array->length == 0) {
        dai_error("IndexError: pop from empty list\n");
        assert(false);
    }
    DaiValue value = array->elements[array->length - 1];
    array->length--;
    return value;
}

static DaiValue
DaiObjArray_sub(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 1 && argc != 2) {
        dai_error("TypeError: sub() expected 1-2 arguments, but got %d\n", argc);
        assert(false);
    }
    if (!IS_INTEGER(argv[0])) {
        dai_error("TypeError: sub() expected int arguments, but got %s\n", dai_value_ts(argv[0]));
        assert(false);
    }
    if (argc == 2 && !IS_INTEGER(argv[1])) {
        dai_error("TypeError: sub() expected int arguments, but got %s\n", dai_value_ts(argv[1]));
        assert(false);
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
        dai_error("TypeError: remove() expected 1 arguments, but got %d\n", argc);
        assert(false);
    }
    DaiObjArray* array = AS_ARRAY(receiver);
    DaiValue value     = argv[0];
    for (int i = 0; i < array->length; i++) {
        if (dai_value_equal(array->elements[i], value)) {
            for (int j = i; j < array->length - 1; j++) {
                array->elements[j] = array->elements[j + 1];
            }
            array->length--;
            return receiver;
        }
    }
    dai_error("ValueError: array.remove(x): x not in array\n");
    assert(false);
}

static DaiValue
DaiObjArray_removeIndex(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc,
                        DaiValue* argv) {
    if (argc != 1) {
        dai_error("TypeError: removeIndex() expected 1 arguments, but got %d\n", argc);
        assert(false);
    }
    if (!IS_INTEGER(argv[0])) {
        dai_error("TypeError: removeIndex() expected int arguments, but got %s\n",
                  dai_value_ts(argv[0]));
        assert(false);
    }
    DaiObjArray* array = AS_ARRAY(receiver);
    int index          = AS_INTEGER(argv[0]);
    if (index < 0) {
        index += array->length;
    }
    if (index < 0 || index >= array->length) {
        dai_error("IndexError: removeIndex() index out of bounds\n");
        assert(false);
    }
    for (int i = index; i < array->length - 1; i++) {
        array->elements[i] = array->elements[i + 1];
    }
    array->length--;
    return NIL_VAL;
}

static DaiValue
DaiObjArray_extend(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 1) {
        dai_error("TypeError: extend() expected 1 arguments, but got %d\n", argc);
        assert(false);
    }
    if (!IS_ARRAY(argv[0])) {
        dai_error("TypeError: extend() expected array arguments, but got %s\n",
                  dai_value_ts(argv[0]));
        assert(false);
    }
    DaiObjArray* array = AS_ARRAY(receiver);
    DaiObjArray* other = AS_ARRAY(argv[0]);
    int want           = array->length + other->length;
    if (want > array->capacity) {
        int old_capacity = array->capacity;
        array->capacity  = GROW_CAPACITY(array->capacity);
        if (want > array->capacity) {
            array->capacity = want;
        }
        array->elements = GROW_ARRAY(DaiValue, array->elements, old_capacity, array->capacity);
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
        dai_error("TypeError: has() expected 1 arguments, but got %d\n", argc);
        assert(false);
    }
    DaiObjArray* array = AS_ARRAY(receiver);
    DaiValue value     = argv[0];
    for (int i = 0; i < array->length; i++) {
        if (dai_value_equal(array->elements[i], value)) {
            return dai_true;
        }
    }
    return dai_false;
}

static DaiValue
DaiObjArray_reversed(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc,
                     DaiValue* argv) {
    if (argc != 0) {
        dai_error("TypeError: reversed() expected 0 arguments, but got %d\n", argc);
        assert(false);
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
        dai_error("TypeError: reverse() expected 0 arguments, but got %d\n", argc);
        assert(false);
    }
    DaiObjArray* array = AS_ARRAY(receiver);
    DaiObjArray_preverse(array);
    return receiver;
}

static DaiValue
DaiObjArray_sort(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 1) {
        dai_error("TypeError: sort() expected 1 arguments, but got %d\n", argc);
        assert(false);
    }
    DaiObjArray* array = AS_ARRAY(receiver);
    DaiValue cmp       = argv[0];
    // insertion sort
    for (int i = 1; i < array->length; i++) {
        DaiValue val = array->elements[i];
        int j        = i - 1;
        for (; j >= 0; --j) {
            DaiValue ret = DaiVM_runCall(vm, cmp, 2, array->elements[j], val);
            if (!IS_INTEGER(ret)) {
                dai_error("TypeError: cmp() expected int return value, but got %s\n",
                          dai_value_ts(ret));
                assert(false);
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
    DaiObjArrayFunctionNo_add,
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
            {.type = DaiObjType_builtinFn},
            .name     = "length",
            .function = &DaiObjArray_length,
        },
    [DaiObjArrayFunctionNo_add] =
        {
            {.type = DaiObjType_builtinFn},
            .name     = "add",
            .function = &DaiObjArray_add,
        },
    [DaiObjArrayFunctionNo_pop] =
        {
            {.type = DaiObjType_builtinFn},
            .name     = "pop",
            .function = &DaiObjArray_pop,
        },
    [DaiObjArrayFunctionNo_sub] =
        {
            {.type = DaiObjType_builtinFn},
            .name     = "sub",
            .function = &DaiObjArray_sub,
        },
    [DaiObjArrayFunctionNo_remove] =
        {
            {.type = DaiObjType_builtinFn},
            .name     = "remove",
            .function = &DaiObjArray_remove,
        },
    [DaiObjArrayFunctionNo_removeIndex] =
        {
            {.type = DaiObjType_builtinFn},
            .name     = "removeIndex",
            .function = &DaiObjArray_removeIndex,
        },
    [DaiObjArrayFunctionNo_extend] =
        {
            {.type = DaiObjType_builtinFn},
            .name     = "extend",
            .function = &DaiObjArray_extend,
        },
    [DaiObjArrayFunctionNo_has] =
        {
            {.type = DaiObjType_builtinFn},
            .name     = "has",
            .function = &DaiObjArray_has,
        },
    [DaiObjArrayFunctionNo_reversed] =
        {
            {.type = DaiObjType_builtinFn},
            .name     = "reversed",
            .function = &DaiObjArray_reversed,
        },
    [DaiObjArrayFunctionNo_reverse] =
        {
            {.type = DaiObjType_builtinFn},
            .name     = "reverse",
            .function = &DaiObjArray_reverse,
        },
    [DaiObjArrayFunctionNo_sort] =
        {
            {.type = DaiObjType_builtinFn},
            .name     = "sort",
            .function = &DaiObjArray_sort,
        },
};

static DaiValue
DaiObjArray_get_property(DaiVM* vm, DaiValue receiver, DaiObjString* name) {
    const char* cname = name->chars;
    switch (cname[0]) {
        case 'a': {
            if (strcmp(cname, "add") == 0) {
                return OBJ_VAL(&DaiObjArrayBuiltins[DaiObjArrayFunctionNo_add]);
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
    dai_error(
        "PropertyError: '%s' object has no property '%s'\n", dai_value_ts(receiver), name->chars);
    assert(false);
}

DaiObjArray*
DaiObjArray_New(DaiVM* vm, const DaiValue* elements, const int length) {
    DaiObjArray* array           = ALLOCATE_OBJ(vm, DaiObjArray, DaiObjType_array);
    array->obj.get_property_func = DaiObjArray_get_property;
    array->capacity              = length;
    array->length                = length;
    array->elements              = NULL;
    if (length > 0) {
        array->elements = GROW_ARRAY(DaiValue, NULL, 0, length);
        memcpy(array->elements, elements, length * sizeof(DaiValue));
    }
    return array;
}

static DaiValue
DaiObjArray_subscriptGet(__attribute__((unused)) DaiVM* vm, DaiValue receiver, DaiValue index) {
    assert(IS_ARRAY(receiver));
    if (!IS_INTEGER(index)) {
        // todo return error
        dai_error("array index must be integer\n");
        assert(false);
    }
    const DaiObjArray* array = AS_ARRAY(receiver);
    int64_t n                = AS_INTEGER(index);
    if (n < 0) {
        n += array->length;
    }
    if (n < 0 || n >= array->length) {
        // todo return error
        dai_error("array index out of bounds\n");
        assert(false);
    }
    return array->elements[n];
}

static DaiValue
DaiObjArray_subscriptSet(__attribute__((unused)) DaiVM* vm, DaiValue receiver, DaiValue index,
                         DaiValue value) {
    assert(IS_ARRAY(receiver));
    if (!IS_INTEGER(index)) {
        // todo return error
        dai_error("array index must be integer\n");
        assert(false);
    }
    const DaiObjArray* array = AS_ARRAY(receiver);
    int64_t n                = AS_INTEGER(index);
    if (n < 0) {
        n += array->length;
    }
    if (n < 0 || n >= array->length) {
        // todo return error
        dai_error("array index out of bounds\n");
        assert(false);
    }
    array->elements[n] = value;
    return receiver;
}


// #endregion


DaiObjError*
DaiObjError_Newf(DaiVM* vm, const char* format, ...) {
    DaiObjError* error = ALLOCATE_OBJ(vm, DaiObjError, DaiObjType_error);
    va_list args;
    va_start(args, format);
    vsnprintf(error->message, sizeof(error->message), format, args);
    va_end(args);
    return error;
}

static void
print_function(DaiObjFunction* function) {
    if (function->name == NULL) {
        dai_log("<script>");
        return;
    }
    dai_log("<fn %s>", function->name->chars);
}
void
dai_print_object(DaiValue value) {
    switch (OBJ_TYPE(value)) {
        case DaiObjType_boundMethod: {
            DaiObjBoundMethod* bound_method = AS_BOUND_METHOD(value);
            dai_log("<bound method %s of <object at %p>>",
                    DaiObjClosure_name(bound_method->method),
                    bound_method);
            break;
        }
        case DaiObjType_class: {
            dai_log("%s", AS_CLASS(value)->name->chars);
            break;
        }
        case DaiObjType_instance: {
            DaiObjInstance* instance = AS_INSTANCE(value);
            dai_log("<instance of %s at %p>", instance->klass->name->chars, instance);
            break;
        }
        case DaiObjType_function: print_function(AS_FUNCTION(value)); break;
        case DaiObjType_string: dai_log("%s", AS_CSTRING(value)); break;
        case DaiObjType_builtinFn: dai_log("<builtin-fn %s>", AS_BUILTINFN(value)->name); break;
        case DaiObjType_closure: {
            print_function(AS_CLOSURE(value)->function);
            break;
        }
        case DaiObjType_array: {
            dai_log("[");
            for (int i = 0; i < AS_ARRAY(value)->length; i++) {
                dai_print_value(AS_ARRAY(value)->elements[i]);
                dai_log(", ");
            }
            dai_log("]");
            break;
        }
        case DaiObjType_error: dai_log("%s", AS_ERROR(value)->message); break;
        default: dai_log("Unknown object type %d", OBJ_TYPE(value)); break;
    }
}

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
        case DaiObjType_error: return "error";
        default: return "unknown";
    }
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
        // todo 将错误也作为一种值
        dai_error("wrong number of arguments. got=%d, want=1\n", argc);
        assert(false);
    }
    const DaiValue arg = argv[0];
    if (IS_STRING(arg)) {
        return INTEGER_VAL(AS_STRING(arg)->length);
    } else {
        // todo 将错误也作为一种值
        dai_error("TypeError: \"len\" not supported \"%s\"\n", dai_value_ts(arg));
        assert(false);
    }
    return NIL_VAL;
}

static DaiValue
builtin_type(__attribute__((unused)) DaiVM* vm, __attribute__((unused)) DaiValue receiver, int argc,
             DaiValue* argv) {
    if (argc != 1) {
        // todo 将错误也作为一种值
        dai_error("wrong number of arguments. got=%d, want=1\n", argc);
        assert(false);
    }
    const DaiValue arg = argv[0];
    const char* s      = dai_value_ts(arg);
    return OBJ_VAL(dai_copy_string(vm, s, strlen(s)));
}

DaiObjBuiltinFunction builtin_funcs[256] = {
    {
        {.type = DaiObjType_builtinFn},
        .name     = "print",
        .function = builtin_print,
    },
    {
        {.type = DaiObjType_builtinFn},
        .name     = "len",
        .function = builtin_len,
    },
    {
        {.type = DaiObjType_builtinFn},
        .name     = "type",
        .function = builtin_type,
    },

    {
        .name = NULL,
    },
};

// #endregion
