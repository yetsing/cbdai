/*
栈虚拟机中的值
*/
#ifndef B2545ED5_584D_4263_BD5B_7D98D76B99E3
#define B2545ED5_584D_4263_BD5B_7D98D76B99E3

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int capacity;
    int count;
    void** values;
} DaiPtrArray;
void
DaiPtrArray_init(DaiPtrArray* array);
void
DaiPtrArray_write(DaiPtrArray* array, void* value);
void
DaiPtrArray_reset(DaiPtrArray* array);
bool
DaiPtrArray_contains(DaiPtrArray* array, void* value);

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

/**
 * @brief 打印值
 *
 * @param value 值
 */
void
dai_print_value(DaiValue value);

/**
 * @brief 比较两个值是否相等
 *
 * @param a 值
 * @param b 值
 * @param limit 递归深度限制
 *
 * @return 1 相等，0 不相等， -1 错误
 */
int
dai_value_equal_with_limit(DaiValue a, DaiValue b, int* limit);

/**
 * @brief 比较两个值是否相等
 *
 * @param a 值
 * @param b 值
 *
 * @return 1 相等，0 不相等， -1 错误
 */
int
dai_value_equal(DaiValue a, DaiValue b);

/**
 * @brief 返回值的字符串表示
 *
 * @note  返回的字符串由调用者释放
 *
 * @param value 值
 * @param visited 访问过的对象，用于检测循环引用
 *
 * @return char* 字符串
 */
char*
dai_value_string_with_visited(DaiValue value, DaiPtrArray* visited);

/**
 * @brief 返回值的字符串表示
 *
 * @note  返回的字符串由调用者释放
 *
 * @param value 值
 *
 * @return char* 字符串
 */
char*
dai_value_string(DaiValue value);

/**
 * @brief 返回值是否为真
 *
 * @param value 值
 *
 * @return true 真，false 假
 */
bool
dai_value_is_truthy(const DaiValue value);

/**
 * @brief 返回值的哈希值
 *
 * @param value 值
 *
 * @return uint64_t 哈希值
 */
uint64_t
dai_value_hash(DaiValue value, uint64_t seed0, uint64_t seed1);

/**
 * @brief 返回值是否可哈希
 *
 * @param value 值
 *
 * @return true 可哈希，false 不可哈希
 */
bool
dai_value_is_hashable(DaiValue value);

/**
 * @brief 返回值是否可迭代
 *
 * @param value 值
 *
 * @return true 可迭代，false 不可迭代
 */
bool
dai_value_is_iterable(DaiValue value);


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

DaiValue*
DaiValueArray_raw_copy(DaiValueArray* src);

#endif /* B2545ED5_584D_4263_BD5B_7D98D76B99E3 */
