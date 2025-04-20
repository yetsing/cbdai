#include "dai_objects/dai_object_map.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "hashmap.h"

#include "dai_memory.h"
#include "dai_stringbuffer.h"
#include "dai_vm.h"

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
        vm, "'map' object has not property '%s'", name->chars);
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

static struct DaiObjOperation map_operation = {
    .get_property_func  = DaiObjMap_get_property,
    .set_property_func  = NULL,
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

static DaiValue
dai_iter_init_return_self(DaiVM* vm, DaiValue receiver) {
    return receiver;
}

static struct DaiObjOperation map_iterator_operation = {
    .get_property_func  = NULL,
    .set_property_func  = NULL,
    .subscript_get_func = NULL,
    .subscript_set_func = NULL,
    .string_func        = dai_default_string_func,
    .equal_func         = dai_default_equal,
    .hash_func          = dai_default_hash,
    .iter_init_func     = dai_iter_init_return_self,
    .iter_next_func     = DaiObjMapIterator_iter_next,
    .get_method_func    = NULL,
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
