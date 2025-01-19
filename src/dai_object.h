#ifndef D772959F_7E3C_4B7E_B19F_7880710F99C0
#define D772959F_7E3C_4B7E_B19F_7880710F99C0

#include "dai_chunk.h"
#include "dai_table.h"
#include "dai_value.h"

typedef struct _DaiVM DaiVM;
typedef struct _DaiObjClass DaiObjClass;

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_BOUND_METHOD(value) dai_is_obj_type(value, DaiObjType_boundMethod)
#define IS_INSTANCE(value) dai_is_obj_type(value, DaiObjType_instance)
#define IS_CLOSURE(value) dai_is_obj_type(value, DaiObjType_closure)
#define IS_FUNCTION(value) dai_is_obj_type(value, DaiObjType_function)
#define IS_BUILTINFN(value) dai_is_obj_type(value, DaiObjType_builtinFn)
#define IS_CLASS(value) dai_is_obj_type(value, DaiObjType_class)
#define IS_STRING(value) dai_is_obj_type(value, DaiObjType_string)
#define IS_ARRAY(value) dai_is_obj_type(value, DaiObjType_array)
#define IS_ERROR(value) dai_is_obj_type(value, DaiObjType_error)

#define AS_BOUND_METHOD(value) ((DaiObjBoundMethod*)AS_OBJ(value))
#define AS_INSTANCE(value) ((DaiObjInstance*)AS_OBJ(value))
#define AS_CLOSURE(value) ((DaiObjClosure*)AS_OBJ(value))
#define AS_FUNCTION(value) ((DaiObjFunction*)AS_OBJ(value))
#define AS_BUILTINFN(value) ((DaiObjBuiltinFunction*)AS_OBJ(value))
#define AS_CLASS(value) ((DaiObjClass*)AS_OBJ(value))
#define AS_STRING(value) ((DaiObjString*)AS_OBJ(value))
#define AS_CSTRING(value) (((DaiObjString*)AS_OBJ(value))->chars)
#define AS_ARRAY(value) ((DaiObjArray*)AS_OBJ(value))
#define AS_ERROR(value) ((DaiObjError*)AS_OBJ(value))

typedef enum {
    DaiObjType_function,
    DaiObjType_closure,
    DaiObjType_string,
    DaiObjType_builtinFn,
    DaiObjType_class,
    DaiObjType_instance,
    DaiObjType_boundMethod,
    DaiObjType_array,
    DaiObjType_error,
} DaiObjType;

typedef DaiValue (*BuiltinFn)(DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv);
typedef DaiValue (*GetPropertyFn)(DaiVM* vm, DaiValue receiver, DaiObjString* name);
typedef DaiValue (*SetPropertyFn)(DaiVM* vm, DaiValue receiver, DaiObjString* name, DaiValue value);
typedef DaiValue (*SubscriptGetFn)(DaiVM* vm, DaiValue receiver, DaiValue index);
typedef DaiValue (*SubscriptSetFn)(DaiVM* vm, DaiValue receiver, DaiValue index, DaiValue value);
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

struct DaiOperation {
    GetPropertyFn get_property_func;
    SetPropertyFn set_property_func;
    SubscriptGetFn subscript_get_func;
    SubscriptSetFn subscript_set_func;
    StringFn string_func;   // caller should free the returned string
    EqualFn equal_func;
};

struct DaiObj {
    DaiObjType type;
    bool is_marked;        // 是否被标记（标记-清除垃圾回收算法）
    struct DaiObj* next;   // 对象链表，会串起所有分配的对象
    struct DaiOperation* operation;
};

typedef struct {
    DaiObj obj;
    int arity;   // 参数数量
    DaiChunk chunk;
    DaiObjString* name;
    DaiObjClass* superclass;   // 用于实现 super
} DaiObjFunction;
DaiObjFunction*
DaiObjFunction_New(DaiVM* vm, const char* name, const char* filename);
const char*
DaiObjFunction_name(DaiObjFunction* function);

typedef struct {
    DaiObj obj;
    DaiObjFunction* function;
    DaiValue* frees;   // 函数引用的自由变量
    int free_count;
} DaiObjClosure;
DaiObjClosure*
DaiObjClosure_New(DaiVM* vm, DaiObjFunction* function);
const char*
DaiObjClosure_name(DaiObjClosure* closure);

typedef struct _DaiObjClass {
    DaiObj obj;
    DaiObjString* name;
    DaiTable class_fields;       // 类属性
    DaiTable class_methods;      // 类方法
    DaiTable methods;            // 实例方法
    DaiTable fields;             // 实例属性
    DaiValueArray field_names;   // 按定义顺序存储实例属性名
    DaiObjClass* parent;
    // 下面是一些特殊的实例方法
    DaiValue init;   // 自定义的实例初始化方法
} DaiObjClass;
DaiObjClass*
DaiObjClass_New(DaiVM* vm, DaiObjString* name);
DaiValue
DaiObjClass_call(DaiObjClass* klass, DaiVM* vm, int argc, DaiValue* argv);


typedef struct {
    DaiObj obj;
    DaiObjClass* klass;
    DaiTable fields;   // 实例属性
} DaiObjInstance;
DaiObjInstance*
DaiObjInstance_New(DaiVM* vm, DaiObjClass* klass);

typedef struct {
    DaiObj obj;
    DaiValue receiver;
    DaiObjClosure* method;
} DaiObjBoundMethod;
DaiObjBoundMethod*
DaiObjBoundMethod_New(DaiVM* vm, DaiValue receiver, DaiValue method);
DaiObjBoundMethod*
DaiObjClass_get_super_method(DaiVM* vm, DaiObjClass* klass, DaiObjString* name, DaiValue receiver);
void
DaiObj_get_method(DaiVM* vm, DaiObjClass* klass, DaiValue receiver, DaiObjString* name,
                  DaiValue* method);

typedef struct {
    DaiObj obj;
    const char* name;
    BuiltinFn function;
} DaiObjBuiltinFunction;

struct DaiObjString {
    DaiObj obj;
    int length;   // bytes length
    int utf8_length;
    char* chars;
    uint32_t hash;
};

DaiObjString*
dai_take_string(DaiVM* vm, char* chars, int length);
DaiObjString*
dai_copy_string(DaiVM* vm, const char* chars, int length);

typedef struct {
    DaiObj obj;
    int capacity;
    int length;
    DaiValue* elements;
} DaiObjArray;
DaiObjArray*
DaiObjArray_New(DaiVM* vm, const DaiValue* elements, int length);
DaiObjArray*
DaiObjArray_append1(DaiVM* vm, DaiObjArray* array, int n, DaiValue* values);
DaiObjArray*
DaiObjArray_append2(DaiVM* vm, DaiObjArray* array, int n, ...);

typedef struct {
    DaiObj obj;
    char message[256];
} DaiObjError;
DaiObjError*
DaiObjError_Newf(DaiVM* vm, const char* format, ...) __attribute__((format(printf, 2, 3)));

const char*
dai_object_ts(DaiValue value);

static inline bool
dai_is_obj_type(DaiValue value, DaiObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

// 内置函数
extern DaiObjBuiltinFunction builtin_funcs[256];
#endif /* D772959F_7E3C_4B7E_B19F_7880710F99C0 */
