#include "dai_objects/dai_object_tuple.h"

// #region tuple
#include <assert.h>
#include <string.h>

#include "dai_memory.h"
#include "dai_objects/dai_object_array.h"
#include "dai_objects/dai_object_error.h"
#include "dai_stringbuffer.h"

static DaiValue
DaiObjTuple_subscript_get(__attribute__((unused)) DaiVM* vm, DaiValue receiver, DaiValue index) {
    if (!IS_INTEGER(index)) {
        DaiObjError* err = DaiObjError_Newf(vm, "tuple index must be integer");
        return OBJ_VAL(err);
    }
    const DaiObjTuple* tuple = AS_TUPLE(receiver);
    int64_t n                = AS_INTEGER(index);
    if (n < 0) {
        n += tuple->values.count;
    }
    if (n < 0 || n >= tuple->values.count) {
        DaiObjError* err = DaiObjError_Newf(vm, "tuple index out of range");
        return OBJ_VAL(err);
    }
    return tuple->values.values[n];
}

static char*
DaiObjTuple_String(DaiValue value, __attribute__((unused)) DaiPtrArray* visited) {
    if (DaiPtrArray_contains(visited, AS_OBJ(value))) {
        return strdup("(...)");
    }
    DaiPtrArray_write(visited, AS_OBJ(value));
    DaiStringBuffer* sb = DaiStringBuffer_New();
    DaiObjArray* array  = AS_ARRAY(value);
    DaiStringBuffer_write(sb, "(");
    for (int i = 0; i < array->length; i++) {
        char* s = dai_value_string_with_visited(array->elements[i], visited);
        DaiStringBuffer_write(sb, s);
        free(s);
        if (i != array->length - 1) {
            DaiStringBuffer_write(sb, ", ");
        }
    }
    DaiStringBuffer_write(sb, ")");
    return DaiStringBuffer_getAndFree(sb, NULL);
}

static struct DaiObjOperation tuple_operation = {
    .get_property_func  = NULL,
    .set_property_func  = NULL,
    .subscript_get_func = DaiObjTuple_subscript_get,
    .subscript_set_func = NULL,
    .string_func        = DaiObjTuple_String,
    .equal_func         = dai_default_equal,
    .hash_func          = dai_default_hash,
    .iter_init_func     = NULL,
    .iter_next_func     = NULL,
    .get_method_func    = NULL,
};

DaiObjTuple*
DaiObjTuple_New(DaiVM* vm) {
    DaiObjTuple* tuple   = ALLOCATE_OBJ(vm, DaiObjTuple, DaiObjType_tuple);
    tuple->obj.operation = &tuple_operation;
    DaiValueArray_init(&tuple->values);
    return tuple;
}

void
DaiObjTuple_Free(DaiVM* vm, DaiObjTuple* tuple) {
    DaiValueArray_reset(&tuple->values);
    VM_FREE(vm, DaiObjTuple, tuple);
}
void
DaiObjTuple_append(DaiObjTuple* tuple, DaiValue value) {
    DaiValueArray_write(&tuple->values, value);
}

int
DaiObjTuple_length(DaiObjTuple* tuple) {
    return tuple->values.count;
}

DaiValue
DaiObjTuple_get(DaiObjTuple* tuple, int index) {
    assert(index >= 0 && index < tuple->values.count);
    return tuple->values.values[index];
}

void
DaiObjTuple_set(DaiObjTuple* tuple, int index, DaiValue value) {
    assert(index >= 0 && index < tuple->values.count);
    tuple->values.values[index] = value;
}
// #endregion
