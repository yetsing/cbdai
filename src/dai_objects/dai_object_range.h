#ifndef DAI_OBJECT_RANGE_H
#define DAI_OBJECT_RANGE_H

#include "dai_objects/dai_object_base.h"

typedef struct {
    DaiObj obj;
    int64_t start;
    int64_t end;
    int64_t step;
    int64_t curr;
    int64_t index;
} DaiObjRangeIterator;
DaiObjRangeIterator*
DaiObjRangeIterator_New(DaiVM* vm, int64_t start, int64_t end, int64_t step);


#endif //DAI_OBJECT_RANGE_H
