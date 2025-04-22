#include "dai_objects/dai_object_base.h"

#include <stdio.h>
#include <string.h>

#include "dai_memory.h"
#include "dai_vm.h"
#include "dai_windows.h"


char*
dai_default_string_func(DaiValue value, __attribute__((unused)) DaiPtrArray* visited) {
    char buf[64];
    int length = snprintf(buf, sizeof(buf), "<obj at %p>", AS_OBJ(value));
    return strndup(buf, length);
}

int
dai_default_equal(DaiValue a, DaiValue b, __attribute__((unused)) int* limit) {
    return AS_OBJ(a) == AS_OBJ(b);
}

uint64_t
dai_default_hash(DaiValue value) {
    return (uint64_t)(uintptr_t)AS_OBJ(value);
}


DaiObj*
allocate_object(DaiVM* vm, size_t size, DaiObjType type) {
    DaiObj* object    = (DaiObj*)vm_reallocate(vm, NULL, 0, size);
    object->type      = type;
    object->is_marked = false;
    object->next      = vm->objects;
    object->operation = NULL;
    vm->objects       = object;
#ifdef DEBUG_LOG_GC
    dai_log("%p allocate %zu for %d\n", (void*)object, size, type);

#endif

    return object;
}
