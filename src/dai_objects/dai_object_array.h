#ifndef SRC_DAI_OBJECT_ARRAY_H_
#define SRC_DAI_OBJECT_ARRAY_H_

#include "dai_objects/dai_object_base.h"


typedef struct {
    DaiObj obj;
    int capacity;
    int length;
    DaiValue* elements;
} DaiObjArray;
DaiObjArray*
DaiObjArray_New(DaiVM* vm, const DaiValue* elements, const int length);
DaiObjArray*
DaiObjArray_New2(DaiVM* vm, const DaiValue* elements, const int length, const int capacity);
DaiObjArray*
DaiObjArray_append1(DaiVM* vm, DaiObjArray* array, int n, DaiValue* values);
DaiObjArray*
DaiObjArray_append2(DaiVM* vm, DaiObjArray* array, int n, ...);

typedef struct {
    DaiObj obj;
    int index;
    DaiObjArray* array;
} DaiObjArrayIterator;
DaiObjArrayIterator*
DaiObjArrayIterator_New(DaiVM* vm, DaiObjArray* array);

#endif /* SRC_DAI_OBJECT_ARRAY_H_ */