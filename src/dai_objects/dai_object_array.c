#include "dai_objects/dai_object_array.h"

#include <stdarg.h>

#include "dai_memory.h"
#include "dai_stringbuffer.h"

// #region 数组 DaiObjArray
static void
DaiObjArray_shrink(DaiObjArray* array) {
    if (array->length > (array->capacity >> 2)) {
        return;
    }
    int old_capacity = array->capacity;
    array->capacity  = array->capacity >> 1;
    array->elements  = GROW_ARRAY(DaiValue, array->elements, old_capacity, array->capacity);
}

static void
DaiObjArray_grow(DaiObjArray* array, int want) {
    int old_capacity = array->capacity;
    array->capacity  = GROW_CAPACITY(array->capacity);
    if (want > array->capacity) {
        array->capacity = want;
    }
    array->elements = GROW_ARRAY(DaiValue, array->elements, old_capacity, array->capacity);
}

static DaiObjArray*
DaiObjArray_pcopy(DaiVM* vm, DaiObjArray* array) {
    DaiObjArray* copy = DaiObjArray_New(vm, NULL, 0);
    copy->length      = array->length;
    copy->capacity    = array->capacity;
    copy->elements    = GROW_ARRAY(DaiValue, NULL, 0, copy->capacity);
    for (int i = 0; i < array->length; i++) {
        copy->elements[i] = array->elements[i];
    }
    return copy;
}

static void
DaiObjArray_preverse(DaiObjArray* array) {
    for (int i = 0; i < array->length / 2; i++) {
        DaiValue tmp                           = array->elements[i];
        array->elements[i]                     = array->elements[array->length - i - 1];
        array->elements[array->length - i - 1] = tmp;
    }
}

static DaiValue
DaiObjArray_length(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 0) {
        DaiObjError* err = DaiObjError_Newf(vm, "length() expected no arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    return INTEGER_VAL(AS_ARRAY(receiver)->length);
}

static DaiValue
DaiObjArray_append(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc == 0) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "add() expected one or more arguments, but got no arguments");
        return OBJ_VAL(err);
    }
    DaiObjArray* array = AS_ARRAY(receiver);
    DaiObjArray_append1(vm, array, argc, argv);
    return receiver;
}

static DaiValue
DaiObjArray_pop(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 0) {
        DaiObjError* err = DaiObjError_Newf(vm, "pop() expected no arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    DaiObjArray* array = AS_ARRAY(receiver);
    if (array->length == 0) {
        DaiObjError* err = DaiObjError_Newf(vm, "pop from empty array");
        return OBJ_VAL(err);
    }
    DaiValue value = array->elements[array->length - 1];
    array->length--;
    DaiObjArray_shrink(array);
    return value;
}

static DaiValue
DaiObjArray_sub(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 1 && argc != 2) {
        DaiObjError* err = DaiObjError_Newf(vm, "sub() expected 1-2 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (!IS_INTEGER(argv[0])) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "sub() expected int arguments, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
    if (argc == 2 && !IS_INTEGER(argv[1])) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "sub() expected int arguments, but got %s", dai_value_ts(argv[1]));
        return OBJ_VAL(err);
    }
    DaiObjArray* array = AS_ARRAY(receiver);
    int start          = AS_INTEGER(argv[0]);
    int end            = argc == 2 ? AS_INTEGER(argv[1]) : array->length;
    if (start < 0) {
        start += array->length;
        if (start < 0) {
            start = 0;
        }
    }
    if (end < 0) {
        end += array->length;
        if (end < 0) {
            end = 0;
        }
    } else if (end > array->length) {
        end = array->length;
    }
    if (start >= end) {
        return OBJ_VAL(DaiObjArray_New(vm, NULL, 0));
    }
    DaiObjArray* sub_array = DaiObjArray_New(vm, array->elements + start, end - start);
    return OBJ_VAL(sub_array);
}

static DaiValue
DaiObjArray_remove(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err = DaiObjError_Newf(vm, "remove() expected 1 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    DaiObjArray* array = AS_ARRAY(receiver);
    DaiValue value     = argv[0];
    for (int i = 0; i < array->length; i++) {
        int ret = dai_value_equal(array->elements[i], value);
        if (ret == -1) {
            DaiObjError* err =
                DaiObjError_Newf(vm, "maximum recursion depth exceeded in comparison");
            return OBJ_VAL(err);
        }
        if (ret == 1) {
            for (int j = i; j < array->length - 1; j++) {
                array->elements[j] = array->elements[j + 1];
            }
            array->length--;
            DaiObjArray_shrink(array);
            return receiver;
        }
    }
    DaiObjError* err = DaiObjError_Newf(vm, "array.remove(x): x not in array");
    return OBJ_VAL(err);
}

static DaiValue
DaiObjArray_removeIndex(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc,
                        DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "removeIndex() expected 1 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (!IS_INTEGER(argv[0])) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "removeIndex() expected int arguments, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
    DaiObjArray* array = AS_ARRAY(receiver);
    int index          = AS_INTEGER(argv[0]);
    if (index < 0) {
        index += array->length;
    }
    if (index < 0 || index >= array->length) {
        DaiObjError* err = DaiObjError_Newf(vm, "removeIndex() index out of range");
        return OBJ_VAL(err);
    }
    for (int i = index; i < array->length - 1; i++) {
        array->elements[i] = array->elements[i + 1];
    }
    array->length--;
    DaiObjArray_shrink(array);
    return NIL_VAL;
}

static DaiValue
DaiObjArray_extend(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err = DaiObjError_Newf(vm, "extend() expected 1 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (!IS_ARRAY(argv[0])) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "extend() expected array arguments, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
    DaiObjArray* array = AS_ARRAY(receiver);
    DaiObjArray* other = AS_ARRAY(argv[0]);
    int want           = array->length + other->length;
    if (want > array->capacity) {
        DaiObjArray_grow(array, want);
    }
    for (int i = 0; i < other->length; i++) {
        array->elements[array->length + i] = other->elements[i];
    }
    array->length += other->length;
    return receiver;
}

static DaiValue
DaiObjArray_has(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err = DaiObjError_Newf(vm, "has() expected 1 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    DaiObjArray* array = AS_ARRAY(receiver);
    DaiValue value     = argv[0];
    for (int i = 0; i < array->length; i++) {
        int ret = dai_value_equal(array->elements[i], value);
        if (ret == -1) {
            DaiObjError* err =
                DaiObjError_Newf(vm, "maximum recursion depth exceeded in comparison");
            return OBJ_VAL(err);
        }
        if (ret == 1) {
            return dai_true;
        }
    }
    return dai_false;
}

static DaiValue
DaiObjArray_reversed(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc,
                     DaiValue* argv) {
    if (argc != 0) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "reversed() expected 0 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    DaiObjArray* array = AS_ARRAY(receiver);
    // copy array and reverse
    DaiObjArray* copy = DaiObjArray_pcopy(vm, array);
    DaiObjArray_preverse(copy);
    return OBJ_VAL(copy);
}

static DaiValue
DaiObjArray_reverse(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc,
                    DaiValue* argv) {
    if (argc != 0) {
        DaiObjError* err = DaiObjError_Newf(vm, "reverse() expected 0 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    DaiObjArray* array = AS_ARRAY(receiver);
    DaiObjArray_preverse(array);
    return receiver;
}

static DaiValue
DaiObjArray_sort(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err = DaiObjError_Newf(vm, "sort() expected 1 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    DaiObjArray* array = AS_ARRAY(receiver);
    DaiValue cmp       = argv[0];
    // insertion sort
    for (int i = 1; i < array->length; i++) {
        DaiValue val = array->elements[i];
        int j        = i - 1;
        for (; j >= 0; --j) {
            DaiValue ret = DaiVM_runCall(vm, cmp, 2, array->elements[j], val);
            if (IS_ERROR(ret)) {
                return ret;
            }
            if (!IS_INTEGER(ret)) {
                DaiObjError* err = DaiObjError_Newf(
                    vm, "sort cmp() expected int return value, but got %s", dai_value_ts(ret));
                return OBJ_VAL(err);
            }
            if (AS_INTEGER(ret) > 0) {
                array->elements[j + 1] = array->elements[j];   // 数据移动
            } else {
                break;
            }
        }
        array->elements[j + 1] = val;
    }
    return receiver;
}

static DaiValue
DaiObjArray_find(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err = DaiObjError_Newf(vm, "sort() expected 1 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    DaiObjArray* array = AS_ARRAY(receiver);
    DaiValue value     = argv[0];
    for (int i = 0; i < array->length; i++) {
        int ret = dai_value_equal(array->elements[i], value);
        if (ret == -1) {
            DaiObjError* err =
                DaiObjError_Newf(vm, "maximum recursion depth exceeded in comparison");
            return OBJ_VAL(err);
        }
        if (ret == 1) {
            return INTEGER_VAL(i);
        }
    }
    return INTEGER_VAL(-1);
}


enum DaiObjArrayFunctionNo {
    DaiObjArrayFunctionNo_length = 0,
    DaiObjArrayFunctionNo_append,
    DaiObjArrayFunctionNo_pop,
    DaiObjArrayFunctionNo_sub,
    DaiObjArrayFunctionNo_remove,
    DaiObjArrayFunctionNo_removeIndex,
    DaiObjArrayFunctionNo_extend,
    DaiObjArrayFunctionNo_has,
    DaiObjArrayFunctionNo_reversed,
    DaiObjArrayFunctionNo_reverse,
    DaiObjArrayFunctionNo_sort,
    DaiObjArrayFunctionNo_find,
};

static DaiObjBuiltinFunction DaiObjArrayBuiltins[] = {
    [DaiObjArrayFunctionNo_length] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "length",
            .function = &DaiObjArray_length,
        },
    [DaiObjArrayFunctionNo_append] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "append",
            .function = &DaiObjArray_append,
        },
    [DaiObjArrayFunctionNo_pop] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "pop",
            .function = &DaiObjArray_pop,
        },
    [DaiObjArrayFunctionNo_sub] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "sub",
            .function = &DaiObjArray_sub,
        },
    [DaiObjArrayFunctionNo_remove] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "remove",
            .function = &DaiObjArray_remove,
        },
    [DaiObjArrayFunctionNo_removeIndex] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "removeIndex",
            .function = &DaiObjArray_removeIndex,
        },
    [DaiObjArrayFunctionNo_extend] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "extend",
            .function = &DaiObjArray_extend,
        },
    [DaiObjArrayFunctionNo_has] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "has",
            .function = &DaiObjArray_has,
        },
    [DaiObjArrayFunctionNo_reversed] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "reversed",
            .function = &DaiObjArray_reversed,
        },
    [DaiObjArrayFunctionNo_reverse] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "reverse",
            .function = &DaiObjArray_reverse,
        },
    [DaiObjArrayFunctionNo_sort] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "sort",
            .function = &DaiObjArray_sort,
        },
    [DaiObjArrayFunctionNo_find] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "find",
            .function = &DaiObjArray_find,
        },
};

static DaiValue
DaiObjArray_get_property(DaiVM* vm, DaiValue receiver, DaiObjString* name) {
    const char* cname = name->chars;
    switch (cname[0]) {
        case 'a': {
            if (strcmp(cname, "append") == 0) {
                return OBJ_VAL(&DaiObjArrayBuiltins[DaiObjArrayFunctionNo_append]);
            }
            break;
        }
        case 'e': {
            if (strcmp(cname, "extend") == 0) {
                return OBJ_VAL(&DaiObjArrayBuiltins[DaiObjArrayFunctionNo_extend]);
            }
            break;
        }
        case 'f': {
            if (strcmp(cname, "find") == 0) {
                return OBJ_VAL(&DaiObjArrayBuiltins[DaiObjArrayFunctionNo_find]);
            }
            break;
        }
        case 'h': {
            if (strcmp(cname, "has") == 0) {
                return OBJ_VAL(&DaiObjArrayBuiltins[DaiObjArrayFunctionNo_has]);
            }
            break;
        }
        case 'l': {
            if (strcmp(cname, "length") == 0) {
                return OBJ_VAL(&DaiObjArrayBuiltins[DaiObjArrayFunctionNo_length]);
            }
            break;
        }
        case 'p': {
            if (strcmp(cname, "pop") == 0) {
                return OBJ_VAL(&DaiObjArrayBuiltins[DaiObjArrayFunctionNo_pop]);
            }
            break;
        }
        case 'r': {
            if (strcmp(cname, "remove") == 0) {
                return OBJ_VAL(&DaiObjArrayBuiltins[DaiObjArrayFunctionNo_remove]);
            }
            if (strcmp(cname, "removeIndex") == 0) {
                return OBJ_VAL(&DaiObjArrayBuiltins[DaiObjArrayFunctionNo_removeIndex]);
            }
            if (strcmp(cname, "reversed") == 0) {
                return OBJ_VAL(&DaiObjArrayBuiltins[DaiObjArrayFunctionNo_reversed]);
            }
            if (strcmp(cname, "reverse") == 0) {
                return OBJ_VAL(&DaiObjArrayBuiltins[DaiObjArrayFunctionNo_reverse]);
            }
            break;
        }
        case 's': {
            if (strcmp(cname, "sub") == 0) {
                return OBJ_VAL(&DaiObjArrayBuiltins[DaiObjArrayFunctionNo_sub]);
            }
            if (strcmp(cname, "sort") == 0) {
                return OBJ_VAL(&DaiObjArrayBuiltins[DaiObjArrayFunctionNo_sort]);
            }
            break;
        }
    }
    DaiObjError* err = DaiObjError_Newf(
        vm, "'%s' object has not property '%s'", dai_value_ts(receiver), name->chars);
    return OBJ_VAL(err);
}

static int
DaiObjArray_equal(DaiValue a, DaiValue b, int* limit) {
    DaiObjArray* array_a = AS_ARRAY(a);
    DaiObjArray* array_b = AS_ARRAY(b);
    if (array_a == array_b) {
        return true;
    }
    if (array_a->length != array_b->length) {
        return false;
    }
    for (int i = 0; i < array_a->length; i++) {
        int ret = dai_value_equal_with_limit(array_a->elements[i], array_b->elements[i], limit);
        if (ret == -1) {
            return -1;
        }
        if (ret == 0) {
            return false;
        }
    }
    return true;
}

static DaiValue
DaiObjArray_subscript_get(__attribute__((unused)) DaiVM* vm, DaiValue receiver, DaiValue index) {
    assert(IS_ARRAY(receiver));
    if (!IS_INTEGER(index)) {
        DaiObjError* err = DaiObjError_Newf(vm, "array index must be integer");
        return OBJ_VAL(err);
    }
    const DaiObjArray* array = AS_ARRAY(receiver);
    int64_t n                = AS_INTEGER(index);
    if (n < 0) {
        n += array->length;
    }
    if (n < 0 || n >= array->length) {
        DaiObjError* err = DaiObjError_Newf(vm, "array index out of range");
        return OBJ_VAL(err);
    }
    return array->elements[n];
}

static DaiValue
DaiObjArray_subscript_set(__attribute__((unused)) DaiVM* vm, DaiValue receiver, DaiValue index,
                          DaiValue value) {
    assert(IS_ARRAY(receiver));
    if (!IS_INTEGER(index)) {
        DaiObjError* err = DaiObjError_Newf(vm, "array index must be integer");
        return OBJ_VAL(err);
    }
    const DaiObjArray* array = AS_ARRAY(receiver);
    int64_t n                = AS_INTEGER(index);
    if (n < 0) {
        n += array->length;
    }
    if (n < 0 || n >= array->length) {
        DaiObjError* err = DaiObjError_Newf(vm, "array index out of range");
        return OBJ_VAL(err);
    }
    array->elements[n] = value;
    return receiver;
}

static char*
DaiObjArray_String(DaiValue value, DaiPtrArray* visited) {
    if (DaiPtrArray_contains(visited, AS_OBJ(value))) {
        return strdup("[...]");
    }
    DaiPtrArray_write(visited, AS_OBJ(value));
    DaiStringBuffer* sb = DaiStringBuffer_New();
    DaiObjArray* array  = AS_ARRAY(value);
    DaiStringBuffer_write(sb, "[");
    for (int i = 0; i < array->length; i++) {
        char* s = dai_value_string_with_visited(array->elements[i], visited);
        DaiStringBuffer_write(sb, s);
        free(s);
        if (i != array->length - 1) {
            DaiStringBuffer_write(sb, ", ");
        }
    }
    DaiStringBuffer_write(sb, "]");
    return DaiStringBuffer_getAndFree(sb, NULL);
}

static DaiValue
DaiObjArray_iter_init(__attribute__((unused)) DaiVM* vm, DaiValue receiver) {
    DaiObjArrayIterator* iterator = DaiObjArrayIterator_New(vm, AS_ARRAY(receiver));
    return OBJ_VAL(iterator);
}

static struct DaiObjOperation array_operation = {
    .get_property_func  = DaiObjArray_get_property,
    .set_property_func  = NULL,
    .subscript_get_func = DaiObjArray_subscript_get,
    .subscript_set_func = DaiObjArray_subscript_set,
    .string_func        = DaiObjArray_String,
    .equal_func         = DaiObjArray_equal,
    .hash_func          = NULL,
    .iter_init_func     = DaiObjArray_iter_init,
    .iter_next_func     = NULL,
    .get_method_func    = DaiObjArray_get_property,
};

DaiObjArray*
DaiObjArray_New2(DaiVM* vm, const DaiValue* elements, const int length, const int capacity) {
    DaiObjArray* array   = ALLOCATE_OBJ(vm, DaiObjArray, DaiObjType_array);
    array->obj.operation = &array_operation;
    array->capacity      = capacity;
    array->length        = length;
    array->elements      = NULL;
    if (capacity > 0) {
        array->elements = GROW_ARRAY(DaiValue, NULL, 0, capacity);
    }
    if (elements != NULL) {
        memcpy(array->elements, elements, length * sizeof(DaiValue));
    }
    return array;
}

DaiObjArray*
DaiObjArray_New(DaiVM* vm, const DaiValue* elements, const int length) {
    return DaiObjArray_New2(vm, elements, length, length);
}


DaiObjArray*
DaiObjArray_append1(DaiVM* vm, DaiObjArray* array, int n, DaiValue* values) {
    int want = array->length + n;
    if (want > array->capacity) {
        DaiObjArray_grow(array, want);
    }
    for (int i = 0; i < n; i++) {
        array->elements[array->length + i] = values[i];
    }
    array->length += n;
    return array;
}

DaiObjArray*
DaiObjArray_append2(DaiVM* vm, DaiObjArray* array, int n, ...) {
    va_list args;
    va_start(args, n);
    int want = array->length + n;
    if (want > array->capacity) {
        DaiObjArray_grow(array, want);
    }
    for (int i = 0; i < n; i++) {
        DaiValue value                     = va_arg(args, DaiValue);
        array->elements[array->length + i] = value;
    }
    array->length += n;
    va_end(args);
    return array;
}


// #endregion

// #region DaiObjArrayIterator

static DaiValue
DaiObjArrayIterator_iter_next(__attribute__((unused)) DaiVM* vm, DaiValue receiver, DaiValue* index,
                              DaiValue* element) {
    DaiObjArrayIterator* iterator = AS_ARRAY_ITERATOR(receiver);
    if (iterator->index >= iterator->array->length) {
        return UNDEFINED_VAL;
    }
    *index   = INTEGER_VAL(iterator->index);
    *element = iterator->array->elements[iterator->index];
    iterator->index++;
    return NIL_VAL;
}

static DaiValue
dai_iter_init_return_self(DaiVM* vm, DaiValue receiver) {
    return receiver;
}

static struct DaiObjOperation array_iterator_operation = {
    .get_property_func  = NULL,
    .set_property_func  = NULL,
    .subscript_get_func = NULL,
    .subscript_set_func = NULL,
    .string_func        = dai_default_string_func,
    .equal_func         = dai_default_equal,
    .hash_func          = dai_default_hash,
    .iter_init_func     = dai_iter_init_return_self,
    .iter_next_func     = DaiObjArrayIterator_iter_next,
    .get_method_func    = NULL,
};

DaiObjArrayIterator*
DaiObjArrayIterator_New(DaiVM* vm, DaiObjArray* array) {
    DaiObjArrayIterator* iterator = ALLOCATE_OBJ(vm, DaiObjArrayIterator, DaiObjType_arrayIterator);
    iterator->obj.operation       = &array_iterator_operation;
    iterator->array               = array;
    iterator->index               = 0;
    return iterator;
}

// #endregion
