#include "dai_object_class.h"

#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "hashmap.h"

#include "dai_objects/dai_object_function.h"
#include "dai_memory.h"
#include "dai_stringbuffer.h"
#include "dai_table.h"
#include "dai_value.h"
#include "dai_vm.h"

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

static struct DaiObjOperation class_operation = {
    .get_property_func  = DaiObjClass_get_property,
    .set_property_func  = DaiObjClass_set_property,
    .subscript_get_func = NULL,
    .subscript_set_func = NULL,
    .string_func        = DaiObjClass_String,
    .equal_func         = dai_default_equal,
    .hash_func          = dai_default_hash,
    .iter_init_func     = NULL,
    .iter_next_func     = NULL,
    .get_method_func    = DaiObjClass_get_method,
};

#define STRING_NAME(name) dai_copy_string_intern(vm, name, strlen(name))

static DaiValue
DaiObjClass_builtin_init(DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    DaiObjInstance* instance = AS_INSTANCE(receiver);
    DaiObjClass* klass       = instance->klass;
    for (int i = 0; i < argc; i++) {
        DaiObjString* field_name = AS_STRING(DaiObjTuple_get(klass->define_field_names, i));
        const void* res          = hashmap_get_with_hash(
            klass->fields, &(DaiFieldDesc){.name = field_name}, field_name->hash);
        assert(res != NULL);
        instance->fields[((DaiFieldDesc*)res)->index] = argv[i];
    }
    return receiver;
}

static DaiObjBuiltinFunction builtin_init = {
    {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
    .name     = "__init__",
    .function = DaiObjClass_builtin_init,
};

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
#ifdef _WIN32
    DaiTable_set(&klass->methods, name, OBJ_VAL(&builtin_init));
    klass->init_fn = OBJ_VAL(&builtin_init);
#else
    DaiObjClass_define_method(klass, STRING_NAME("__init__"), OBJ_VAL(&builtin_init));
#endif
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
    if (IS_BUILTINFN(instance->klass->init_fn)) {
        const BuiltinFn func  = AS_BUILTINFN(instance->klass->init_fn)->function;
        const DaiValue result = func(vm, OBJ_VAL(instance), argc, argv);
        if (DAI_IS_ERROR(result)) {
            return result;
        }
    } else {
        // 设置此次调用的实例
        vm->stack_top[-argc - 1] = OBJ_VAL(instance);
        DaiValue res             = DaiVM_runCall2(vm, instance->klass->init_fn, argc);
        if (DAI_IS_ERROR(res)) {
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
    const void* res =
        hashmap_get_with_hash(klass->fields, &(DaiFieldDesc){.name = name}, name->hash);
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
        for (int j = 0; j < count; j++) {
            DaiObjTuple_set(
                klass->define_field_names, j, DaiObjTuple_get(parent->define_field_names, j));
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

static struct DaiObjOperation instance_operation = {
    .get_property_func  = DaiObjInstance_get_property,
    .set_property_func  = DaiObjInstance_set_property,
    .subscript_get_func = NULL,
    .subscript_set_func = NULL,
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
        instance->fields = ALLOCATE(DaiValue, instance->field_count);
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

DaiObjInstance*
DaiObjInstance_Copy(DaiVM* vm, DaiObjInstance* instance) {
    DaiObjInstance* new_instance = ALLOCATE_OBJ(vm, DaiObjInstance, DaiObjType_instance);
    new_instance->obj.operation  = &instance_operation;
    new_instance->klass          = instance->klass;
    new_instance->initialized    = instance->initialized;
    new_instance->field_count    = instance->field_count;
    new_instance->fields         = ALLOCATE(DaiValue, instance->field_count);
    if (new_instance->fields == NULL) {
        dai_error("DaiObjInstance_Copy: Out of memory\n");
        abort();
    }
    memcpy(new_instance->fields, instance->fields, sizeof(DaiValue) * instance->field_count);
    return new_instance;
}

void
DaiObjInstance_set(DaiObjInstance* instance, DaiObjString* name, DaiValue value) {
    DaiFieldDesc prop = {.name = name};
    const void* res   = hashmap_get_with_hash(instance->klass->fields, &prop, name->hash);
    if (res) {
        const DaiFieldDesc* propp      = res;
        instance->fields[propp->index] = value;
    }
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

static struct DaiObjOperation bound_method_operation = {
    .get_property_func  = NULL,
    .set_property_func  = NULL,
    .subscript_get_func = NULL,
    .subscript_set_func = NULL,
    .string_func        = DaiObjBoundMethod_String,
    .equal_func         = dai_default_equal,
    .hash_func          = dai_default_hash,
    .iter_init_func     = NULL,
    .iter_next_func     = NULL,
    .get_method_func    = NULL,
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
