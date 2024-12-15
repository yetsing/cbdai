/*
内存操作封装，用于编译和运行时
*/

#include <stdlib.h>

#include "dai_memory.h"

#ifdef DEBUG_LOG_GC
#    include "dai_debug.h"
#    include <stdio.h>
#endif

#define GC_HEAP_GROW_FACTOR 2

void*
vm_reallocate(DaiVM* vm, void* pointer, size_t old_size, size_t new_size) {
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
    return reallocate(pointer, old_size, new_size);
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

static void
vm_free_object(DaiVM* vm, DaiObj* object) {
#ifdef DEBUG_LOG_GC
    printf("%p free type %d\n", (void*)object, object->type);
#endif
    switch (object->type) {
        case DaiObjType_array: {
            DaiObjArray* array = (DaiObjArray*)object;
            for (int i = 0; i < array->length; i++) {
                if (IS_OBJ(array->elements[i])) {
                    vm_free_object(vm, AS_OBJ(array->elements[i]));
                }
            }
            VM_FREE_ARRAY(vm, DaiValue, array->elements, array->capacity);
            VM_FREE(vm, DaiObjArray, object);
            break;
        }
        case DaiObjType_boundMethod: {
            VM_FREE(vm, DaiObjBoundMethod, object);
            break;
        }
        case DaiObjType_instance: {
            DaiObjInstance* instance = (DaiObjInstance*)object;
            DaiTable_reset(&instance->fields);
            VM_FREE(vm, DaiObjInstance, object);
            break;
        }
        case DaiObjType_class: {
            DaiTable_reset(&((DaiObjClass*)object)->class_fields);
            DaiTable_reset(&((DaiObjClass*)object)->class_methods);
            DaiTable_reset(&((DaiObjClass*)object)->methods);
            DaiTable_reset(&((DaiObjClass*)object)->fields);
            DaiValueArray_reset(&((DaiObjClass*)object)->field_names);
            VM_FREE(vm, DaiObjClass, object);
            break;
        }
        case DaiObjType_closure: {
            DaiObjClosure* closure = (DaiObjClosure*)object;
            VM_FREE_ARRAY(vm, DaiValue, closure->frees, closure->free_count);
            VM_FREE(vm, DaiObjClosure, object);
            break;
        }
        case DaiObjType_function: {
            DaiObjFunction* function = (DaiObjFunction*)object;
            DaiChunk_reset(&function->chunk);
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
        default: {
            unreachable();
            break;
        };
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
void
markObject(DaiVM* vm, DaiObj* object) {
    if (object == NULL) {
        return;
    }
    if (object->is_marked) {
        return;
    }
#ifdef DEBUG_LOG_GC
    printf("%p mark ", (void*)object);
    dai_print_value(OBJ_VAL(object));
    printf("\n");
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

static void
blackenObject(DaiVM* vm, DaiObj* object) {
#ifdef DEBUG_LOG_GC
    printf("%p blacken ", (void*)object);
    dai_print_value(OBJ_VAL(object));
    printf("\n");
#endif
    switch (object->type) {
        case DaiObjType_array: {
            const DaiObjArray* array = (DaiObjArray*)object;
            for (int i = 0; i < array->length; i++) {
                if (IS_OBJ(array->elements[i])) {
                    blackenObject(vm, AS_OBJ(array->elements[i]));
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
            markTable(vm, &instance->fields);
            break;
        }
        case DaiObjType_class: {
            DaiObjClass* klass = (DaiObjClass*)object;
            markObject(vm, (DaiObj*)klass->name);
            markTable(vm, &klass->class_fields);
            markTable(vm, &klass->class_methods);
            markTable(vm, &klass->methods);
            markTable(vm, &klass->fields);
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
            break;
        }
        case DaiObjType_builtinFn:
        case DaiObjType_string: {
            break;
        }
    }
}

static void
markRoots(DaiVM* vm) {
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
    // 标记全局变量
    {
        int count = DaiSymbolTable_count(vm->globalSymbolTable);
        for (int i = 0; i < count; i++) {
            markValue(vm, vm->globals[i]);
        }
    }
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
    printf("-- gc begin\n");
    size_t before = vm->bytesAllocated;
#endif
    markRoots(vm);
    traceReferences(vm);
    // 清除字符串表中对回收对象的引用
    tableRemoveWhite(&vm->strings);
    sweep(vm);
    vm->nextGC = vm->bytesAllocated * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_LOG_GC
    printf("-- gc end\n");
    printf("   collected %zu bytes (from %zu to %zu) next at %zu\n",
           before - vm->bytesAllocated,
           before,
           vm->bytesAllocated,
           vm->nextGC);
#endif
}

#ifdef TEST
void
test_mark(DaiVM* vm) {
    markRoots(vm);
    traceReferences(vm);
}
#endif
// #endregion
