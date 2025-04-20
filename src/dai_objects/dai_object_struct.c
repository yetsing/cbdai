#include "dai_object_struct.h"

#include <string.h>
#include <stdio.h>

#include "dai_memory.h"

static char*
DaiObjStruct_String(DaiValue value, __attribute__((unused)) DaiPtrArray* visited) {
    char buf[64];
    DaiObjStruct* obj = AS_STRUCT(value);
    int length        = snprintf(buf, sizeof(buf), "<struct %s>", obj->name);
    return strndup(buf, length);
}

DaiObjStruct*
DaiObjStruct_New(DaiVM* vm, const char* name, struct DaiObjOperation* operation, size_t size,
                 void (*free)(DaiObjStruct* udata)) {
    DaiObjStruct* obj  = ALLOCATE_OBJ1(vm, DaiObjStruct, DaiObjType_struct, size);
    obj->obj.operation = operation;
    if (operation->string_func == NULL) {
        operation->string_func = DaiObjStruct_String;
    }
    obj->name = name;
    obj->size = size;
    obj->free = free;
    return obj;
}
void
DaiObjStruct_Free(DaiVM* vm, DaiObjStruct* obj) {
    if (obj->free != NULL) {
        obj->free(obj);
    }
    VM_FREE1(vm, obj->size, obj);
}
