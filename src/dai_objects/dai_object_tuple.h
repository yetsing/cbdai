#ifndef CBDAI_DAI_OBJECT_TUPLE_H
#define CBDAI_DAI_OBJECT_TUPLE_H

#include "dai_objects/dai_object_base.h"

typedef struct {
    DaiObj obj;
    DaiValueArray values;
} DaiObjTuple;
DaiObjTuple*
DaiObjTuple_New(DaiVM* vm);
void
DaiObjTuple_Free(DaiVM* vm, DaiObjTuple* tuple);
void
DaiObjTuple_append(DaiObjTuple* tuple, DaiValue value);
int
DaiObjTuple_length(DaiObjTuple* tuple);
DaiValue
DaiObjTuple_get(DaiObjTuple* tuple, int index);
void
DaiObjTuple_set(DaiObjTuple* tuple, int index, DaiValue value);

#endif /* CBDAI_DAI_OBJECT_TUPLE_H */
