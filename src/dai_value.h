/*
栈虚拟机中的值
*/
#ifndef B2545ED5_584D_4263_BD5B_7D98D76B99E3
#define B2545ED5_584D_4263_BD5B_7D98D76B99E3

#include <stdbool.h>
#include <stdint.h>

typedef struct DaiObj DaiObj;
typedef struct DaiObjString DaiObjString;

typedef enum {
    DaiValueType_undefined,   // 仅供内部使用，不会暴露给用户
    DaiValueType_nil,
    DaiValueType_bool,
    DaiValueType_int,
    DaiValueType_float,
    DaiValueType_obj,
} DaiValueType;

typedef struct {
    DaiValueType type;
    union {
        int64_t intval;
        double floatval;
        bool boolean;
        DaiObj* obj;
    } as;
} DaiValue;

// 返回类型的字符串表示
const char*
dai_value_ts(DaiValue value);

#define IS_UNDEFINED(value) ((value).type == DaiValueType_undefined)
#define IS_BOOL(value) ((value).type == DaiValueType_bool)
#define IS_NIL(value) ((value).type == DaiValueType_nil)
#define IS_INTEGER(value) ((value).type == DaiValueType_int)
#define IS_NOT_INTEGER(value) ((value).type != DaiValueType_int)
#define IS_FLOAT(value) ((value).type == DaiValueType_float)
#define IS_OBJ(value) ((value).type == DaiValueType_obj)

#define AS_BOOL(value) ((value).as.boolean)
#define AS_INTEGER(value) ((value).as.intval)
#define AS_FLOAT(value) ((value).as.floatval)
#define AS_OBJ(value) ((value).as.obj)

#define UNDEFINED_VAL ((DaiValue){DaiValueType_undefined, {.intval = 0}})
#define BOOL_VAL(value) ((DaiValue){DaiValueType_bool, {.boolean = value}})
#define NIL_VAL ((DaiValue){DaiValueType_nil, {.intval = 0}})
#define INTEGER_VAL(value) ((DaiValue){DaiValueType_int, {.intval = value}})
#define FLOAT_VAL(value) ((DaiValue){DaiValueType_float, {.floatval = value}})
#define OBJ_VAL(object) ((DaiValue){DaiValueType_obj, {.obj = (DaiObj*)object}})

void
dai_print_value(DaiValue value);

bool
dai_value_equal(DaiValue a, DaiValue b);
char*
dai_value_string(DaiValue value);   // caller should free the returned string

typedef struct {
    int capacity;
    int count;
    DaiValue* values;
} DaiValueArray;

void
DaiValueArray_init(DaiValueArray* array);

void
DaiValueArray_write(DaiValueArray* array, DaiValue value);

void
DaiValueArray_reset(DaiValueArray* array);

void
DaiValueArray_copy(DaiValueArray* src, DaiValueArray* dst);

#endif /* B2545ED5_584D_4263_BD5B_7D98D76B99E3 */
