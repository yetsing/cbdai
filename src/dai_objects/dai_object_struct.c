#include "dai_object_struct.h"

#include <stdio.h>
#include <string.h>

#include "dai_memory.h"
#include "dai_value.h"
#include "dai_windows.h"   // IWYU pragma: keep
#include "hashmap.h"

static char*
DaiObjStruct_String(DaiValue value, __attribute__((unused)) DaiPtrArray* visited) {
    char buf[64];
    DaiObjStruct* obj = AS_STRUCT(value);
    int length        = snprintf(buf, sizeof(buf), "<struct %s>", obj->name);
    return strndup(buf, length);
}

DaiObjStruct*
DaiObjStruct_New(DaiVM* vm, const char* name, struct DaiObjOperation* operation, size_t size,
                 void (*free)(DaiVM* vm, DaiObjStruct* udata)) {
    DaiObjStruct* obj  = ALLOCATE_OBJ1(vm, DaiObjStruct, DaiObjType_struct, size);
    obj->obj.operation = operation;
    if (operation->string_func == NULL) {
        operation->string_func = DaiObjStruct_String;
    }
    obj->name      = name;
    obj->size      = size;
    obj->free      = free;
    obj->ref_count = 0;
    return obj;
}
void
DaiObjStruct_Free(DaiVM* vm, DaiObjStruct* obj) {
    if (obj->free != NULL) {
        obj->free(vm, obj);
    }
    VM_FREE1(vm, obj->size, obj);
}

void
DaiObjStruct_add_ref(DaiVM* vm, DaiObjStruct* obj, DaiValue value) {
    assert(obj->ref_count < sizeof(obj->refs) / sizeof(obj->refs[0]));
    obj->refs[obj->ref_count++] = value;
}

// #region SimpleObjectStruct

typedef struct {
    const char* name;
    DaiValue value;
} SimpleObjectEntry;

int
SimpleObjectEntry_compare(const void* a, const void* b, __attribute__((unused)) void* udata) {
    const SimpleObjectEntry* entry_a = (const SimpleObjectEntry*)a;
    const SimpleObjectEntry* entry_b = (const SimpleObjectEntry*)b;
    return strcmp(entry_a->name, entry_b->name);
}

uint64_t
SimpleObjectEntry_hash(const void* item, uint64_t seed0, uint64_t seed1) {
    const SimpleObjectEntry* entry = (const SimpleObjectEntry*)item;
    return hashmap_sip(entry->name, strlen(entry->name), seed0, seed1);
}

// SimpleObjectStruct 是一个简单的对象结构体，
// 它的成员变量是一个哈希表， 用于存储键值对。
// 这个结构体的设计是为了方便存储和访问简单的对象属性。
typedef struct SimpleObjectStruct {
    DAI_OBJ_STRUCT_BASE
    bool readonly;
    struct hashmap* map;
} DaiSimpleObjectStruct;

static DaiValue
SimpleObjectStruct_get_property(DaiVM* vm, DaiValue receiver, DaiObjString* name) {
    DaiSimpleObjectStruct* obj = (DaiSimpleObjectStruct*)AS_STRUCT(receiver);
    SimpleObjectEntry entry    = {.name = name->chars};
    const void* res            = hashmap_get(obj->map, &entry);
    if (res) {
        return ((SimpleObjectEntry*)res)->value;
    }
    DaiObjError* err =
        DaiObjError_Newf(vm, "'%s' object has not property '%s'", obj->name, name->chars);
    return OBJ_VAL(err);
}

static DaiValue
SimpleObjectStruct_set_property(DaiVM* vm, DaiValue receiver, DaiObjString* name, DaiValue value) {
    DaiSimpleObjectStruct* obj = (DaiSimpleObjectStruct*)AS_STRUCT(receiver);
    if (obj->readonly) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "'%s' object is readonly, can not set property '%s'", obj->name, name->chars);
        return OBJ_VAL(err);
    }
    SimpleObjectEntry entry = {.name = name->chars, .value = value};
    if (hashmap_set(obj->map, &entry) == NULL && hashmap_oom(obj->map)) {
        DaiObjError* err = DaiObjError_Newf(vm, "Out of memory");
        return OBJ_VAL(err);
    }
    return value;
}

static char*
SimpleObjectStruct_String(DaiValue value, __attribute__((unused)) DaiPtrArray* visited) {
    char buf[128];
    DaiSimpleObjectStruct* obj = (DaiSimpleObjectStruct*)AS_STRUCT(value);
    int length                 = snprintf(buf, sizeof(buf), "<struct %s(%p)>", obj->name, obj);
    return strndup(buf, length);
}


static struct DaiObjOperation simple_object_struct_operation = {
    .get_property_func  = SimpleObjectStruct_get_property,
    .set_property_func  = SimpleObjectStruct_set_property,
    .subscript_get_func = NULL,
    .subscript_set_func = NULL,
    .string_func        = SimpleObjectStruct_String,
    .equal_func         = dai_default_equal,
    .hash_func          = dai_default_hash,
    .iter_init_func     = NULL,
    .iter_next_func     = NULL,
    .get_method_func    = NULL,
};

static void
SimpleObjectStruct_destructor(DaiVM* vm, DaiObjStruct* st) {
    DaiSimpleObjectStruct* obj = (DaiSimpleObjectStruct*)st;
    hashmap_free(obj->map);
}

DaiSimpleObjectStruct*
DaiSimpleObjectStruct_New(DaiVM* vm, const char* name, bool readonly) {
    DaiSimpleObjectStruct* obj =
        (DaiSimpleObjectStruct*)DaiObjStruct_New(vm,
                                                 name,
                                                 &simple_object_struct_operation,
                                                 sizeof(DaiSimpleObjectStruct),
                                                 SimpleObjectStruct_destructor);

    uint64_t seed0, seed1;
    DaiVM_getSeed2(vm, &seed0, &seed1);
    obj->map = hashmap_new(sizeof(SimpleObjectEntry),
                           0,
                           seed0,
                           seed1,
                           SimpleObjectEntry_hash,
                           SimpleObjectEntry_compare,
                           NULL,
                           vm);
    if (obj->map == NULL) {
        return NULL;
    }
    obj->readonly = readonly;
    return obj;
}


DaiObjError*
DaiSimpleObjectStruct_add(DaiVM* vm, DaiSimpleObjectStruct* obj, const char* name, DaiValue value) {
    SimpleObjectEntry entry = {.name = name, .value = value};
    if (hashmap_set(obj->map, &entry) == NULL && hashmap_oom(obj->map)) {
        DaiObjError* err = DaiObjError_Newf(vm, "Out of memory");
        return err;
    }
    return NULL;
}

// #endregion
