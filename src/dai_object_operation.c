#include "dai_object_operation.h"

#include <stdio.h>
#include <string.h>



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
