/*
栈虚拟机中的值
*/
#include <inttypes.h>
#include <stdio.h>

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
    switch (value.type) {
        case DaiValueType_undefined: dai_log("undefined"); break;
        case DaiValueType_nil: dai_log("nil"); break;
        case DaiValueType_int: dai_log("%" PRId64, value.as.intval); break;
        case DaiValueType_float: dai_log("%f", value.as.floatval); break;
        case DaiValueType_bool:
            if (AS_BOOL(value)) {
                dai_log("true");
            } else {
                dai_log("false");
            }
            break;
        case DaiValueType_obj: {
            dai_print_object(value);
            break;
        }

        default: dai_log("unknown"); break;
    }
}

static bool
float_equals(double a, double b) {
    double epsilon = 1e-10;
    return fabs(a - b) < epsilon;
}

bool
dai_value_equal(DaiValue a, DaiValue b) {
    if (a.type != b.type) {
        return false;
    }
    switch (a.type) {
        case DaiValueType_nil: return true;
        case DaiValueType_int: return AS_INTEGER(a) == AS_INTEGER(b);
        case DaiValueType_float: return float_equals(AS_FLOAT(a), AS_FLOAT(b));
        case DaiValueType_bool: return AS_BOOL(a) == AS_BOOL(b);
        case DaiValueType_obj: return AS_OBJ(a) == AS_OBJ(b);
        default: return true;
    }
}

void
DaiValueArray_init(DaiValueArray* array) {
    array->capcity = 0;
    array->count   = 0;
    array->values  = NULL;
}

void
DaiValueArray_write(DaiValueArray* array, DaiValue value) {
    if (array->capcity < array->count + 1) {
        int old_capacity = array->capcity;
        array->capcity   = GROW_CAPACITY(array->capcity);
        array->values    = GROW_ARRAY(DaiValue, array->values, old_capacity, array->capcity);
    }
    array->values[array->count] = value;
    array->count++;
}

void
DaiValueArray_reset(DaiValueArray* array) {
    FREE_ARRAY(DaiValue, array->values, array->capcity);
    DaiValueArray_init(array);
}

void
DaiValueArray_copy(DaiValueArray* src, DaiValueArray* dst) {
    dst->capcity = src->capcity;
    dst->count   = src->count;
    dst->values  = GROW_ARRAY(DaiValue, dst->values, 0, dst->capcity);
    for (int i = 0; i < src->count; i++) {
        dst->values[i] = src->values[i];
    }
}