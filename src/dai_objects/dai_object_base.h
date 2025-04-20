#ifndef SRC_DAI_OBJECT_BASE_H_
#define SRC_DAI_OBJECT_BASE_H_

#include <stdbool.h>
#include <stddef.h>

#include "dai_value.h"

#define BUILTIN_GLOBALS_COUNT 2


typedef struct _DaiVM DaiVM;
typedef struct _DaiObjClass DaiObjClass;

// #region operation
typedef DaiValue (*BuiltinFn)(DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv);
typedef DaiValue (*GetPropertyFn)(DaiVM* vm, DaiValue receiver, DaiObjString* name);
typedef DaiValue (*SetPropertyFn)(DaiVM* vm, DaiValue receiver, DaiObjString* name, DaiValue value);
typedef DaiValue (*SubscriptGetFn)(DaiVM* vm, DaiValue receiver, DaiValue index);
typedef DaiValue (*SubscriptSetFn)(DaiVM* vm, DaiValue receiver, DaiValue index, DaiValue value);
typedef uint64_t (*HashFn)(DaiValue value);
/**
 * @brief 比较两个值是否相等
 *
 * @param a 值
 * @param b 值
 * @param limit 递归深度限制
 *
 * @return 1 相等，0 不相等， -1 错误
 */
typedef int (*EqualFn)(DaiValue a, DaiValue b, int* limit);
typedef char* (*StringFn)(DaiValue value, DaiPtrArray* visited);
typedef DaiValue (*IterInitFn)(DaiVM* vm, DaiValue receiver);
// 返回 undefined 表示结束
typedef DaiValue (*IterNextFn)(DaiVM* vm, DaiValue receiver, DaiValue* index, DaiValue* element);
typedef DaiValue (*GetMethodFn)(DaiVM* vm, DaiValue receiver, DaiObjString* name);

struct DaiObjOperation {
    GetPropertyFn get_property_func;
    SetPropertyFn set_property_func;
    SubscriptGetFn subscript_get_func;
    SubscriptSetFn subscript_set_func;
    StringFn string_func;   // caller should free the returned string
    EqualFn equal_func;
    HashFn hash_func;
    IterInitFn iter_init_func;
    IterNextFn iter_next_func;
    // 给类和实例快速获取方法用的，搭配 DaiOpCallMethod DaiOpCallSelfMethod 字节码指令
    // （get_property_func 会将方法包成 bound_method 再返回，有很大的性能开销）
    GetMethodFn get_method_func;
};

char*
dai_default_string_func(DaiValue value, __attribute__((unused)) DaiPtrArray* visited);
int
dai_default_equal(DaiValue a, DaiValue b, __attribute__((unused)) int* limit);
uint64_t
dai_default_hash(DaiValue value);

// #endregion

// #region object

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_BOUND_METHOD(value) dai_is_obj_type(value, DaiObjType_boundMethod)
#define IS_INSTANCE(value) dai_is_obj_type(value, DaiObjType_instance)
#define IS_CLOSURE(value) dai_is_obj_type(value, DaiObjType_closure)
#define IS_FUNCTION(value) dai_is_obj_type(value, DaiObjType_function)
#define IS_BUILTINFN(value) dai_is_obj_type(value, DaiObjType_builtinFn)
#define IS_CLASS(value) dai_is_obj_type(value, DaiObjType_class)
#define IS_STRING(value) dai_is_obj_type(value, DaiObjType_string)
#define IS_ARRAY(value) dai_is_obj_type(value, DaiObjType_array)
#define IS_ARRAY_ITERATOR(value) dai_is_obj_type(value, DaiObjType_arrayIterator)
#define IS_MAP(value) dai_is_obj_type(value, DaiObjType_map)
#define IS_MAP_ITERATOR(value) dai_is_obj_type(value, DaiObjType_mapIterator)
#define IS_RANGE_ITERATOR(value) dai_is_obj_type(value, DaiObjType_rangeIterator)
#define IS_ERROR(value) dai_is_obj_type(value, DaiObjType_error)
#define IS_CFUNCTION(value) dai_is_obj_type(value, DaiObjType_cFunction)
#define IS_MODULE(value) dai_is_obj_type(value, DaiObjType_module)
#define IS_TUPLE(value) dai_is_obj_type(value, DaiObjType_tuple)
#define IS_STRUCT(value) dai_is_obj_type(value, DaiObjType_struct)

#define AS_BOUND_METHOD(value) ((DaiObjBoundMethod*)AS_OBJ(value))
#define AS_INSTANCE(value) ((DaiObjInstance*)AS_OBJ(value))
#define AS_CLOSURE(value) ((DaiObjClosure*)AS_OBJ(value))
#define AS_FUNCTION(value) ((DaiObjFunction*)AS_OBJ(value))
#define AS_BUILTINFN(value) ((DaiObjBuiltinFunction*)AS_OBJ(value))
#define AS_CLASS(value) ((DaiObjClass*)AS_OBJ(value))
#define AS_STRING(value) ((DaiObjString*)AS_OBJ(value))
#define AS_CSTRING(value) (((DaiObjString*)AS_OBJ(value))->chars)
#define AS_ARRAY(value) ((DaiObjArray*)AS_OBJ(value))
#define AS_ARRAY_ITERATOR(value) ((DaiObjArrayIterator*)AS_OBJ(value))
#define AS_MAP(value) ((DaiObjMap*)AS_OBJ(value))
#define AS_MAP_ITERATOR(value) ((DaiObjMapIterator*)AS_OBJ(value))
#define AS_RANGE_ITERATOR(value) ((DaiObjRangeIterator*)AS_OBJ(value))
#define AS_ERROR(value) ((DaiObjError*)AS_OBJ(value))
#define AS_CFUNCTION(value) ((DaiObjCFunction*)AS_OBJ(value))
#define AS_MODULE(value) ((DaiObjModule*)AS_OBJ(value))
#define AS_TUPLE(value) ((DaiObjTuple*)AS_OBJ(value))
#define AS_STRUCT(value) ((DaiObjStruct*)AS_OBJ(value))

#define IS_FUNCTION_LIKE(value)                                                               \
    (IS_FUNCTION(value) || IS_CLOSURE(value) || IS_BUILTINFN(value) || IS_CFUNCTION(value) || \
     IS_BOUND_METHOD(value))

typedef enum {
    DaiObjType_function,
    DaiObjType_closure,
    DaiObjType_string,
    DaiObjType_builtinFn,
    DaiObjType_class,
    DaiObjType_instance,
    DaiObjType_boundMethod,
    DaiObjType_array,
    DaiObjType_arrayIterator,
    DaiObjType_map,
    DaiObjType_mapIterator,
    DaiObjType_rangeIterator,
    DaiObjType_error,
    DaiObjType_cFunction,   // c api registered function
    DaiObjType_module,
    DaiObjType_tuple,
    DaiObjType_struct,   // c struct
} DaiObjType;

typedef void (*CFunction)(void* dai);

struct DaiObj {
    DaiObjType type;
    bool is_marked;        // 是否被标记（标记-清除垃圾回收算法）
    struct DaiObj* next;   // 对象链表，会串起所有分配的对象
    struct DaiObjOperation* operation;
};

static inline bool
dai_is_obj_type(DaiValue value, DaiObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#define ALLOCATE_OBJ(vm, type, objectType) (type*)allocate_object(vm, sizeof(type), objectType)
#define ALLOCATE_OBJ1(vm, type, objectType, size) (type*)allocate_object(vm, size, objectType)

DaiObj*
allocate_object(DaiVM* vm, size_t size, DaiObjType type);
// #endregion

#endif /* SRC_DAI_OBJECT_BASE_H_ */
