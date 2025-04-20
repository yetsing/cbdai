#include "dai_objects/dai_object_range.h"


static DaiValue
DaiObjRangeIterator_iter_next(__attribute__((unused)) DaiVM* vm, DaiValue receiver, DaiValue* index,
                              DaiValue* element) {
    DaiObjRangeIterator* iterator = AS_RANGE_ITERATOR(receiver);
    if ((iterator->step >= 0 && iterator->curr >= iterator->end) ||
        (iterator->step < 0 && iterator->curr <= iterator->end)) {
        return UNDEFINED_VAL;
        }
    *index   = INTEGER_VAL(iterator->index);
    *element = INTEGER_VAL(iterator->curr);
    iterator->curr += iterator->step;
    iterator->index++;
    return NIL_VAL;
}

static DaiValue
dai_iter_init_return_self(DaiVM* vm, DaiValue receiver) {
    return receiver;
}

static struct DaiObjOperation range_iterator_operation = {
    .get_property_func  = NULL,
    .set_property_func  = NULL,
    .subscript_get_func = NULL,
    .subscript_set_func = NULL,
    .string_func        = dai_default_string_func,
    .equal_func         = dai_default_equal,
    .hash_func          = dai_default_hash,
    .iter_init_func     = dai_iter_init_return_self,
    .iter_next_func     = DaiObjRangeIterator_iter_next,
    .get_method_func    = NULL,
};

DaiObjRangeIterator*
DaiObjRangeIterator_New(DaiVM* vm, int64_t start, int64_t end, int64_t step) {
    DaiObjRangeIterator* range_iterator =
        ALLOCATE_OBJ(vm, DaiObjRangeIterator, DaiObjType_rangeIterator);
    range_iterator->obj.operation = &range_iterator_operation;
    range_iterator->start         = start;
    range_iterator->end           = end;
    range_iterator->step          = step;
    range_iterator->curr          = start;
    range_iterator->index         = 0;
    return range_iterator;
}