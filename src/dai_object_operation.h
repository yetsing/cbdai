#ifndef SRC_DAI_OBJECT_OPERATION_H_
#define SRC_DAI_OBJECT_OPERATION_H_

#include "dai_value.h"

typedef struct _DaiVM DaiVM;

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

#endif /* SRC_DAI_OBJECT_OPERATION_H_ */