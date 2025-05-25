/*
内存操作封装，用于编译和运行时
*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "dai_chunk.h"
#include "dai_common.h"
#include "dai_memory.h"
#include "dai_object.h"
#include "dai_objects/dai_object_base.h"
#include "dai_value.h"

#ifdef DEBUG_LOG_GC
#    include "dai_debug.h"
#    include <stdio.h>
#endif

#define GC_HEAP_GROW_FACTOR 2

void*
vm_reallocate(DaiVM* vm, void* pointer, size_t old_size, size_t new_size) {
#ifdef DEBUG_LOG_GC
    size_t allocated = vm->bytesAllocated;
#endif
    vm->bytesAllocated += new_size - old_size;
    if (new_size > old_size) {
        // 频繁地运行 GC ，方便找到内存管理 bug
#ifdef DEBUG_STRESS_GC
        collectGarbage(vm);
#endif
        if (vm->bytesAllocated > vm->nextGC) {
            collectGarbage(vm);
        }
    }
    void* newpointer = reallocate(pointer, old_size, new_size);
#ifdef DEBUG_LOG_GC
    dai_loggc("reallocate %p->%p %zu->%zu\n", pointer, newpointer, old_size, new_size);
    if (allocated + new_size < old_size) {
        dai_loggc("reallocate: %zu + %zu < %zu\n", allocated, new_size, old_size);
        fflush(stdout);
        fflush(stderr);
        abort();
    }
#endif
    return newpointer;
}

void*
reallocate(void* pointer, size_t old_size, size_t new_size) {
    if (new_size == 0) {
        free(pointer);
        return NULL;
    }

    void* result = realloc(pointer, new_size);
    if (result == NULL) {
        abort();
    };
    return result;
}

// note: 只需释放自身内存
static void
vm_free_object(DaiVM* vm, DaiObj* object) {
#ifdef DEBUG_LOG_GC
    dai_loggc("%p free type %d\n", (void*)object, object->type);
#endif
    DaiObjType tp = object->type;
    object->type  = 0xFF;
    switch (tp) {
        case DaiObjType_struct: {
            DaiObjStruct* obj = (DaiObjStruct*)object;
            DaiObjStruct_Free(vm, obj);
            break;
        }
        case DaiObjType_cFunction: {
            free(((DaiObjCFunction*)object)->name);
            VM_FREE(vm, DaiObjCFunction, object);
            break;
        }
        case DaiObjType_rangeIterator: {
            VM_FREE(vm, DaiObjRangeIterator, object);
            break;
        }
        case DaiObjType_mapIterator: {
            VM_FREE(vm, DaiObjMapIterator, object);
            break;
        }
        case DaiObjType_arrayIterator: {
            VM_FREE(vm, DaiObjArrayIterator, object);
            break;
        }
        case DaiObjType_map: {
            DaiObjMap* map = (DaiObjMap*)object;
            DaiObjMap_Free(vm, map);
            break;
        }
        case DaiObjType_error: {
            VM_FREE(vm, DaiObjError, object);
            break;
        }
        case DaiObjType_array: {
            DaiObjArray* array = (DaiObjArray*)object;
            FREE_ARRAY(DaiValue, array->elements, array->capacity);
            VM_FREE(vm, DaiObjArray, object);
            break;
        }
        case DaiObjType_boundMethod: {
            VM_FREE(vm, DaiObjBoundMethod, object);
            break;
        }
        case DaiObjType_instance: {
            DaiObjInstance* instance = (DaiObjInstance*)object;
            DaiObjInstance_Free(vm, instance);
            break;
        }
        case DaiObjType_class: {
            DaiObjClass_Free(vm, (DaiObjClass*)object);
            break;
        }
        case DaiObjType_closure: {
            DaiObjClosure* closure = (DaiObjClosure*)object;
            if (closure->frees != NULL) {
                VM_FREE_ARRAY(vm, DaiValue, closure->frees, closure->free_count);
            }
            VM_FREE(vm, DaiObjClosure, object);
            break;
        }
        case DaiObjType_function: {
            DaiObjFunction* function = (DaiObjFunction*)object;
            DaiChunk_reset(&function->chunk);
            if (function->defaults != NULL) {
                VM_FREE_ARRAY(vm, DaiValue, function->defaults, function->default_count);
            }
            VM_FREE(vm, DaiObjFunction, object);
            break;
        }
        case DaiObjType_string: {
            DaiObjString* string = (DaiObjString*)object;
            VM_FREE_ARRAY(vm, char, string->chars, string->length + 1);
            VM_FREE(vm, DaiObjString, object);
            break;
        }
        case DaiObjType_builtinFn: {
            break;
        }
        case DaiObjType_module: {
            DaiObjModule* module = (DaiObjModule*)object;
            DaiObjModule_Free(vm, module);
            break;
        }
        case DaiObjType_tuple: {
            DaiObjTuple_Free(vm, (DaiObjTuple*)object);
            break;
        }
        case DaiObjType_count: {
            unreachable();
            break;
        }
    }
}

void
dai_free_objects(DaiVM* vm) {
    DaiObj* obj = vm->objects;
    while (obj != NULL) {
        DaiObj* next = obj->next;
        vm_free_object(vm, obj);
        obj = next;
    }
    free(vm->grayStack);
    vm->grayStack = NULL;
}

// #region 垃圾回收

// https://readonly.link/books/https://raw.githubusercontent.com/GuoYaxiang/craftinginterpreters_zh/main/book.json/-/26.%E5%9E%83%E5%9C%BE%E5%9B%9E%E6%94%B6.md
void
markObject(DaiVM* vm, DaiObj* object) {
    if (object == NULL) {
        return;
    }
    if (object->is_marked) {
        return;
    }
#ifdef DEBUG_LOG_GC
    dai_loggc("%p mark %s\n", (void*)object, dai_object_ts(OBJ_VAL(object)));
#endif
    object->is_marked = true;
    if (vm->grayCapacity < vm->grayCount + 1) {
        vm->grayCapacity = GROW_CAPACITY(vm->grayCapacity);
        vm->grayStack    = (DaiObj**)realloc(vm->grayStack, sizeof(DaiObj*) * vm->grayCapacity);
        assert(vm->grayStack != NULL);
    }
    vm->grayStack[vm->grayCount] = object;
    vm->grayCount++;
}

void
markValue(DaiVM* vm, DaiValue value) {
    if (IS_OBJ(value)) {
        markObject(vm, AS_OBJ(value));
    }
}

static void
markArray(DaiVM* vm, DaiValueArray* array) {
    for (int i = 0; i < array->count; i++) {
        markValue(vm, array->values[i]);
    }
}

static void
markTable(DaiVM* vm, DaiTable* table) {
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        markObject(vm, (DaiObj*)entry->key);
        markValue(vm, entry->value);
    }
}

// 标记 object 引用的其他对象
static void
blackenObject(DaiVM* vm, DaiObj* object) {
#ifdef DEBUG_LOG_GC
    dai_loggc("%p blacken %s\n", (void*)object, dai_object_ts(OBJ_VAL(object)));
#endif
    switch (object->type) {
        case DaiObjType_struct: {
            DaiObjStruct* obj = (DaiObjStruct*)object;
            for (size_t i = 0; i < obj->ref_count; i++) {
                markValue(vm, obj->refs[i]);
            }
            break;
        }
        case DaiObjType_tuple: {
            DaiObjTuple* tuple = (DaiObjTuple*)object;
            markArray(vm, &tuple->values);
            break;
        }
        case DaiObjType_module: {
            DaiObjModule* module = (DaiObjModule*)object;
            markObject(vm, (DaiObj*)module->name);
            markObject(vm, (DaiObj*)module->filename);
            markArray(vm, &module->chunk.constants);
            DaiObjString* key;
            DaiValue value;
            size_t i = 0;
            while (DaiObjModule_iter(module, &i, &key, &value)) {
                markObject(vm, (DaiObj*)key);
                if (IS_OBJ(value)) {
                    markObject(vm, AS_OBJ(value));
                }
            }
            break;
        }
        case DaiObjType_mapIterator: {
            DaiObjMapIterator* iterator = (DaiObjMapIterator*)object;
            markObject(vm, (DaiObj*)iterator->map);
            break;
        }
        case DaiObjType_arrayIterator: {
            DaiObjArrayIterator* iterator = (DaiObjArrayIterator*)object;
            markObject(vm, (DaiObj*)iterator->array);
            break;
        }
        case DaiObjType_map: {
            DaiObjMap* map = (DaiObjMap*)object;
            DaiValue key, value;
            size_t i = 0;
            while (DaiObjMap_iter(map, &i, &key, &value)) {
                if (IS_OBJ(key)) {
                    markObject(vm, AS_OBJ(key));
                }
                if (IS_OBJ(value)) {
                    markObject(vm, AS_OBJ(value));
                }
            }
            break;
        }
        case DaiObjType_array: {
            const DaiObjArray* array = (DaiObjArray*)object;
            for (int i = 0; i < array->length; i++) {
                if (IS_OBJ(array->elements[i])) {
                    markObject(vm, AS_OBJ(array->elements[i]));
                }
            }
            break;
        }
        case DaiObjType_boundMethod: {
            DaiObjBoundMethod* bound = (DaiObjBoundMethod*)object;
            markValue(vm, bound->receiver);
            markObject(vm, (DaiObj*)bound->method);
            break;
        }
        case DaiObjType_instance: {
            DaiObjInstance* instance = (DaiObjInstance*)object;
            markObject(vm, (DaiObj*)instance->klass);
            for (int i = 0; i < instance->field_count; i++) {
                markValue(vm, instance->fields[i]);
            }
            break;
        }
        case DaiObjType_class: {
            DaiObjClass* klass = (DaiObjClass*)object;
            markObject(vm, (DaiObj*)klass->name);
            {
                void* res;
                size_t i = 0;
                while (hashmap_iter(klass->class_fields, &i, &res)) {
                    DaiFieldDesc* prop = res;
                    markObject(vm, (DaiObj*)prop->name);
                    markValue(vm, prop->value);
                }
            }
            {
                void* res;
                size_t i = 0;
                while (hashmap_iter(klass->fields, &i, &res)) {
                    DaiFieldDesc* prop = res;
                    markObject(vm, (DaiObj*)prop->name);
                    markValue(vm, prop->value);
                }
            }
            markTable(vm, &klass->class_methods);
            markTable(vm, &klass->methods);
            markObject(vm, (DaiObj*)klass->define_field_names);

            markObject(vm, (DaiObj*)klass->parent);
            break;
        }
        case DaiObjType_closure: {
            DaiObjClosure* closure = (DaiObjClosure*)object;
            markObject(vm, (DaiObj*)closure->function);
            for (int i = 0; i < closure->free_count; ++i) {
                markValue(vm, closure->frees[i]);
            }
            break;
        }
        case DaiObjType_function: {
            DaiObjFunction* function = (DaiObjFunction*)object;
            markObject(vm, (DaiObj*)function->name);
            markArray(vm, &(function->chunk.constants));
            for (int i = 0; i < function->default_count; i++) {
                markValue(vm, function->defaults[i]);
            }
            break;
        }
        case DaiObjType_cFunction:
        case DaiObjType_rangeIterator:
        case DaiObjType_error:
        case DaiObjType_builtinFn:
        case DaiObjType_string:
        case DaiObjType_count: {
            break;
        }
    }
}

static void
markRoots(DaiVM* vm) {
    // 标记内置对象
    for (int i = 0; i < BUILTIN_OBJECT_MAX_COUNT; i++) {
        markValue(vm, vm->builtin_objects[i]);
    }
    // 标记栈
    for (DaiValue* slot = vm->stack; slot < vm->stack_top; slot++) {
        markValue(vm, *slot);
    }
    // 标记调用栈
    {
        for (int i = 0; i < vm->frameCount; i++) {
            markObject(vm, (DaiObj*)vm->frames[i].closure);
            markObject(vm, (DaiObj*)vm->frames[i].function);
        }
    }
    // 标记临时引用
    markValue(vm, vm->temp_ref);
    // 标记模块表
    markObject(vm, (DaiObj*)vm->modules);
}

static void
traceReferences(DaiVM* vm) {
    while (vm->grayCount > 0) {
        vm->grayCount--;
        DaiObj* object = vm->grayStack[vm->grayCount];
        blackenObject(vm, object);
    }
}

static void
sweep(DaiVM* vm) {
    DaiObj* previous = NULL;
    DaiObj* object   = vm->objects;
    while (object != NULL) {
        if (object->is_marked) {
            object->is_marked = false;
            previous          = object;
            object            = object->next;
        } else {
            DaiObj* unreached = object;
            object            = object->next;
            if (previous != NULL) {
                previous->next = object;
            } else {
                vm->objects = object;
            }
            vm_free_object(vm, unreached);
        }
    }
}

void
collectGarbage(DaiVM* vm) {
    // 只在运行时回收，
    // 因为编译字节码的过程中也会创建对象，这样可以去掉编译期间的回收工作
    if (vm->state != VMState_running) {
        return;
    }
#ifdef DEBUG_LOG_GC
    dai_loggc("-- gc begin\n");
    size_t before = vm->bytesAllocated;
#endif
    markRoots(vm);
    traceReferences(vm);
    // 清除字符串表中对回收对象的引用
    tableRemoveWhite(&vm->strings);
    sweep(vm);
    vm->nextGC = vm->bytesAllocated * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_LOG_GC
    dai_loggc("-- gc end\n");
    dai_loggc("   collected %zu bytes (from %zu to %zu) next at %zu\n",
              before - vm->bytesAllocated,
              before,
              vm->bytesAllocated,
              vm->nextGC);
#endif
}

#ifdef DAI_TEST
void
test_mark(DaiVM* vm) {
    markRoots(vm);
    traceReferences(vm);
}
#endif
// #endregion
