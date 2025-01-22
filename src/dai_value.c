/*
栈虚拟机中的值
*/
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "dai_memory.h"
#include "dai_object.h"
#include "dai_value.h"

#include <math.h>

const char*
dai_value_ts(DaiValue value) {
    switch (value.type) {
        case DaiValueType_undefined: return "undefined";
        case DaiValueType_nil: return "nil";
        case DaiValueType_int: return "int";
        case DaiValueType_float: return "float";
        case DaiValueType_bool: return "bool";
        case DaiValueType_obj: return dai_object_ts(value);
        default: return "unknown";
    }
}

void
dai_print_value(DaiValue value) {
    char* s = dai_value_string(value);
    printf("%s", s);
    free(s);
}

static bool
float_equals(double a, double b) {
    double epsilon = 1e-10;
    return fabs(a - b) < epsilon;
}

int
dai_value_equal_with_limit(DaiValue a, DaiValue b, int* limit) {
    if (*limit <= 0) {
        return -1;
    }
    (*limit)--;
    if (a.type != b.type) {
        return 0;
    }
    switch (a.type) {
        case DaiValueType_nil: return 1;
        case DaiValueType_int: return AS_INTEGER(a) == AS_INTEGER(b);
        case DaiValueType_float: return float_equals(AS_FLOAT(a), AS_FLOAT(b));
        case DaiValueType_bool: return AS_BOOL(a) == AS_BOOL(b);
        case DaiValueType_obj: return AS_OBJ(a)->operation->equal_func(a, b, limit);
        default: return 0;
    }
}

int
dai_value_equal(DaiValue a, DaiValue b) {
    int limit = 256;
    return dai_value_equal_with_limit(a, b, &limit);
}

char*
dai_value_string_with_visited(DaiValue value, DaiPtrArray* visited) {
    switch (value.type) {
        case DaiValueType_nil: return strdup("nil");
        case DaiValueType_int: {
            char buf[64];
            snprintf(buf, sizeof(buf), "%" PRId64, AS_INTEGER(value));
            return strdup(buf);
        }
        case DaiValueType_float: {
            char buf[64];
            snprintf(buf, sizeof(buf), "%f", AS_FLOAT(value));
            return strdup(buf);
        }
        case DaiValueType_bool: return AS_BOOL(value) ? strdup("true") : strdup("false");
        case DaiValueType_obj: {
            return AS_OBJ(value)->operation->string_func(value, visited);
        };
        default: return strdup("unknown");
    }
}

char*
dai_value_string(DaiValue value) {
    DaiPtrArray visited;
    DaiPtrArray_init(&visited);
    char* s = dai_value_string_with_visited(value, &visited);
    DaiPtrArray_reset(&visited);
    return s;
}

bool
dai_value_is_truthy(const DaiValue value) {
    switch (value.type) {
        case DaiValueType_nil: return false;
        case DaiValueType_bool: return AS_BOOL(value);
        case DaiValueType_int: return AS_INTEGER(value) != 0;
        default: return true;
    }
}

uint64_t
dai_value_hash(DaiValue value, __attribute__((unused)) uint64_t seed0,
               __attribute__((unused)) uint64_t seed1) {
    switch (value.type) {
        case DaiValueType_nil: return 0;
        case DaiValueType_int: return (uint64_t)AS_INTEGER(value);
        case DaiValueType_float: return (uint64_t)AS_FLOAT(value);
        case DaiValueType_bool: return AS_BOOL(value) ? 1 : 0;
        case DaiValueType_obj: return AS_OBJ(value)->operation->hash_func(value);
        default: return 0;
    }
}

bool
dai_value_is_hashable(DaiValue value) {
    switch (value.type) {
        case DaiValueType_nil:
        case DaiValueType_int:
        case DaiValueType_float:
        case DaiValueType_bool: return true;
        case DaiValueType_obj: return AS_OBJ(value)->operation->hash_func != NULL;
        default: return false;
    }
}

// #region DaiValueArray

void
DaiValueArray_init(DaiValueArray* array) {
    array->capacity = 0;
    array->count    = 0;
    array->values   = NULL;
}

void
DaiValueArray_write(DaiValueArray* array, DaiValue value) {
    if (array->capacity < array->count + 1) {
        int old_capacity = array->capacity;
        array->capacity  = GROW_CAPACITY(array->capacity);
        array->values    = GROW_ARRAY(DaiValue, array->values, old_capacity, array->capacity);
    }
    array->values[array->count] = value;
    array->count++;
}

void
DaiValueArray_reset(DaiValueArray* array) {
    FREE_ARRAY(DaiValue, array->values, array->capacity);
    DaiValueArray_init(array);
}

void
DaiValueArray_copy(DaiValueArray* src, DaiValueArray* dst) {
    dst->capacity = src->capacity;
    dst->count    = src->count;
    dst->values   = GROW_ARRAY(DaiValue, dst->values, 0, dst->capacity);
    for (int i = 0; i < src->count; i++) {
        dst->values[i] = src->values[i];
    }
}

// #endregion

// #region DaiPtrArray

void
DaiPtrArray_init(DaiPtrArray* array) {
    array->capacity = 0;
    array->count    = 0;
    array->values   = NULL;
}

void
DaiPtrArray_write(DaiPtrArray* array, void* value) {
    if (array->capacity < array->count + 1) {
        int old_capacity = array->capacity;
        array->capacity  = GROW_CAPACITY(array->capacity);
        array->values    = GROW_ARRAY(void*, array->values, old_capacity, array->capacity);
    }
    array->values[array->count] = value;
    array->count++;
}

void
DaiPtrArray_reset(DaiPtrArray* array) {
    FREE_ARRAY(void*, array->values, array->capacity);
    DaiPtrArray_init(array);
}

bool
DaiPtrArray_contains(DaiPtrArray* array, void* value) {
    for (int i = 0; i < array->count; i++) {
        if (array->values[i] == value) {
            return true;
        }
    }
    return false;
}

// #endregion