#include "dai_array.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "dai_malloc.h"

typedef struct DaiArray {
    void* data;
    size_t length;
    size_t capacity;
    size_t element_size;
} DaiArray;

DaiArray*
DaiArray_New(size_t element_size) {
    DaiArray* array     = (DaiArray*)dai_malloc(sizeof(DaiArray));
    array->length       = 0;
    array->capacity     = 8;   // Initial capacity
    array->element_size = element_size;
    array->data         = dai_malloc(array->capacity * element_size);
    return array;
}

void
DaiArray_free(DaiArray* array) {
    free(array->data);
    free(array);
}

void
DaiArray_append(DaiArray* array, void* element) {
    if (array->length == array->capacity) {
        array->capacity *= 2;
        array->data = dai_realloc(array->data, array->capacity * array->element_size);
    }
    memcpy((char*)array->data + array->length * array->element_size, element, array->element_size);
    array->length++;
}

size_t
DaiArray_length(DaiArray* array) {
    return array->length;
}

void*
DaiArray_get(DaiArray* array, int index) {
    if (index < 0) {
        index += array->length;
    }
    assert(index >= 0 && index < array->length);
    return (char*)array->data + index * array->element_size;
}