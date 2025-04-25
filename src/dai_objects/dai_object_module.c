#include "dai_objects/dai_object_module.h"

#include <stdio.h>
#include <string.h>

#include "dai_common.h"
#include "dai_memory.h"
#include "dai_objects/dai_object_error.h"

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
    if (IS_UNDEFINED(*value)) {
        // 如果是未定义的值，说明没有初始化
        DaiObjError* err = DaiObjError_Newf(
            module->vm,
            "'module' object has not property '%s' (most likely due to a circular import)",
            name->chars);
        *value = OBJ_VAL(err);
        return false;
    }
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

static struct DaiObjOperation module_operation = {
    .get_property_func  = DaiObjModule_get_property,
    .set_property_func  = NULL,
    .subscript_get_func = NULL,
    .subscript_set_func = NULL,
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
    module->max_stack_size  = 0;

    module->vm = vm;

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
    size_t count               = hashmap_count(module->global_map);
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
    const DaiPropertyOffset* offset =
        hashmap_get(module->global_map, &(DaiPropertyOffset){.property = property});
    if (offset == NULL) {
        return false;
    }
    if (strcmp(name, "SceneTitle") == 0) {
        printf("DaiObjModule_set_global SceneTitle %d\n", offset->offset);
        printf("    ");
        dai_print_value(value);
        printf("\n");
    }
    module->globals[offset->offset] = value;
    return true;
}

bool
DaiObjModule_add_global(DaiObjModule* module, const char* name, DaiValue value) {
    assert(!(module->compiled));
    DaiObjString* property = dai_copy_string_intern(module->vm, name, strlen(name));
    size_t count           = hashmap_count(module->global_map);
    if (count >= GLOBAL_MAX) {
        dai_error("DaiObjModule_add_global: Too many globals\n");
        abort();
    }
    DaiPropertyOffset offset = {
        .property = property,
        .offset   = count,
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
    if (strcmp(name, "SceneTitle") == 0) {
        printf("DaiObjModule_add_global SceneTitle %zu\n", count);
        printf("    ");
        dai_print_value(value);
        printf("\n");
    }
    module->globals[count] = value;
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

void
builtin_module_setup(DaiObjModule* module) {
    module->compiled = true;
    size_t count     = hashmap_count(module->global_map);
    module->globals  = realloc(module->globals, sizeof(DaiValue) * count);
}
