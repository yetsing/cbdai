#ifndef CBDAI_SRC_DAI_ARRAY_H_
#define CBDAI_SRC_DAI_ARRAY_H_

#include <stddef.h>

typedef struct DaiArray DaiArray;

DaiArray*
DaiArray_New(size_t element_size);
void
DaiArray_free(DaiArray* array);
void
DaiArray_append(DaiArray* array, void* element);
size_t
DaiArray_length(DaiArray* array);
void*
DaiArray_get(DaiArray* array, int index);

#endif /* CBDAI_SRC_DAI_ARRAY_H_ */