#ifndef SRC_DAI_OBJECT_H_
#define SRC_DAI_OBJECT_H_

#include <stdbool.h>
#include <string.h>

#include "hashmap.h"

#include "dai_chunk.h"
#include "dai_object_operation.h"
#include "dai_symboltable.h"
#include "dai_table.h"
#include "dai_value.h"

typedef struct _DaiVM DaiVM;
typedef struct _DaiObjClass DaiObjClass;

#define BUILTIN_GLOBALS_COUNT 2

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

extern struct DaiObjOperation builtin_function_operation;

struct DaiObj {
    DaiObjType type;
    bool is_marked;        // 是否被标记（标记-清除垃圾回收算法）
    struct DaiObj* next;   // 对象链表，会串起所有分配的对象
    struct DaiObjOperation* operation;
};

typedef struct {
    DaiObj obj;
    DaiChunk chunk;
    DaiObjString *name, *filename;
    bool compiled;

    // 全局变量和全局符号表
    struct hashmap* global_map;
    DaiValue* globals;

    int max_local_count;

    DaiVM* vm;
} DaiObjModule;
DaiObjModule*
DaiObjModule_New(DaiVM* vm, const char* name, const char* filename);
void
DaiObjModule_Free(DaiVM* vm, DaiObjModule* module);
void
DaiObjModule_beforeCompile(DaiObjModule* module, DaiSymbolTable* symbol_table);
void
DaiObjModule_afterCompile(DaiObjModule* module, DaiSymbolTable* symbol_table);
bool
DaiObjModule_get_global(DaiObjModule* module, const char* name, DaiValue* value);
bool
DaiObjModule_set_global(DaiObjModule* module, const char* name, DaiValue value);
bool
DaiObjModule_add_global(DaiObjModule* module, const char* name, DaiValue value);
bool
DaiObjModule_iter(DaiObjModule* module, size_t* i, DaiValue* key, DaiValue* value);
void
builtin_module_setup(DaiObjModule* module);

typedef struct {
    DaiObj obj;
    int arity;   // 参数数量
    DaiChunk chunk;
    DaiObjString* name;
    DaiObjClass* superclass;   // 用于实现 super
    DaiValue* defaults;        // 函数参数的默认值
    int default_count;
    DaiObjModule* module;
    int max_local_count;
} DaiObjFunction;
DaiObjFunction*
DaiObjFunction_New(DaiVM* vm, DaiObjModule* module, const char* name, const char* filename);
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

typedef struct {
    DaiObjString* name;
    bool is_const;
    DaiValue value;
    int index;   // 实例属性索引
} DaiFieldDesc;

static inline bool
is_builtin_property(const char* name) {
    // like __xxx__
    size_t name_len = strlen(name);
    return name_len > 4 && name[0] == '_' && name[1] == '_' && name[name_len - 2] == '_' &&
           name[name_len - 1] == '_';
}

typedef struct _DaiObjClass {
    DaiObj obj;
    DaiObjString* name;
    struct hashmap* class_fields;   // 类属性，存储名字 => DaiFieldDesc
    DaiTable class_methods;         // 类方法
    DaiTable methods;               // 实例方法
    struct hashmap* fields;         // 实例属性，存储名字 => DaiFieldDesc
    DaiObjTuple* define_field_names;   // 按定义顺序存储实例属性名（内置类属性 __fields__ 的值）
    DaiObjClass* parent;
    // 下面是一些特殊的实例方法
    DaiValue init_fn;   // __init__ 方法
} DaiObjClass;
DaiObjClass*
DaiObjClass_New(DaiVM* vm, DaiObjString* name);
void
DaiObjClass_Free(DaiVM* vm, DaiObjClass* klass);
DaiValue
DaiObjClass_call(DaiObjClass* klass, DaiVM* vm, int argc, DaiValue* argv);
void
DaiObjClass_define_class_field(DaiObjClass* klass, DaiObjString* name, DaiValue value,
                               bool is_const);
void
DaiObjClass_define_class_method(DaiObjClass* klass, DaiObjString* name, DaiValue value);
int
DaiObjClass_define_field(DaiObjClass* klass, DaiObjString* name, DaiValue value, bool is_const);
void
DaiObjClass_define_method(DaiObjClass* klass, DaiObjString* name, DaiValue value);
void
DaiObjClass_inherit(DaiObjClass* klass, DaiObjClass* parent);


typedef struct {
    DaiObj obj;
    DaiObjClass* klass;
    DaiValue* fields;   // 实例属性
    int field_count;    // 实例属性数量（包括内置属性）
    bool initialized;
} DaiObjInstance;
DaiObjInstance*
DaiObjInstance_New(DaiVM* vm, DaiObjClass* klass);
void
DaiObjInstance_Free(DaiVM* vm, DaiObjInstance* instance);
DaiObjInstance*
DaiObjInstance_Copy(DaiVM* vm, DaiObjInstance* instance);
void
DaiObjInstance_set(DaiObjInstance* instance, DaiObjString* name, DaiValue value);

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

__attribute__((unused)) DaiObjString*
dai_take_string_intern(DaiVM* vm, char* chars, int length);
DaiObjString*
dai_copy_string_intern(DaiVM* vm, const char* chars, int length);
DaiObjString*
dai_take_string(DaiVM* vm, char* chars, int length);
DaiObjString*
dai_copy_string(DaiVM* vm, const char* chars, int length);
int
DaiObjString_cmp(DaiObjString* s1, DaiObjString* s2);

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
    int index;
    DaiObjArray* array;
} DaiObjArrayIterator;
DaiObjArrayIterator*
DaiObjArrayIterator_New(DaiVM* vm, DaiObjArray* array);

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

typedef struct {
    DaiObj obj;
    char message[256];
} DaiObjError;
DaiObjError*
DaiObjError_Newf(DaiVM* vm, const char* format, ...) __attribute__((format(printf, 2, 3)));

typedef struct {
    DaiObj obj;
    struct hashmap* map;
} DaiObjMap;
DaiObjError*
DaiObjMap_New(DaiVM* vm, const DaiValue* values, int length, DaiObjMap** map_ret);
// 释放 map 对象
void
DaiObjMap_Free(DaiVM* vm, DaiObjMap* map);
/**
 * @brief 迭代 map 中的 kv 对
 *
 * @param map map 对象
 * @param i 迭代计数，用来传给 hashmap_iter ，初始值为 0
 * @param key 用于存储 key
 * @param value 用于存储 value
 *
 * @return true 拿到一个 kv 对，false 迭代结束
 */
bool
DaiObjMap_iter(DaiObjMap* map, size_t* i, DaiValue* key, DaiValue* value);
void
DaiObjMap_cset(DaiObjMap* map, DaiValue key, DaiValue value);
bool
DaiObjMap_cget(DaiObjMap* map, DaiValue key, DaiValue* value);

typedef struct {
    DaiObj obj;
    size_t map_index;   // hashmap_iter 的计数
    DaiObjMap* map;
} DaiObjMapIterator;
DaiObjMapIterator*
DaiObjMapIterator_New(DaiVM* vm, DaiObjMap* map);

typedef struct {
    DaiObj obj;
    void* dai;
    BuiltinFn wrapper;
    char* name;
    CFunction func;
    int arity;
} DaiObjCFunction;
DaiObjCFunction*
DaiObjCFunction_New(DaiVM* vm, void* dai, BuiltinFn wrapper, CFunction func, const char* name,
                    int arity);

typedef struct DaiObjStruct DaiObjStruct;
#define DAI_OBJ_STRUCT_BASE \
    DaiObj obj;             \
    const char* name;       \
    size_t size;            \
    void (*free)(DaiObjStruct * st);

typedef struct DaiObjStruct {
    DAI_OBJ_STRUCT_BASE
} DaiObjStruct;
// name 使用字符串字面量，DaiObjStruct 不会管理 name 的内存
DaiObjStruct*
DaiObjStruct_New(DaiVM* vm, const char* name, struct DaiObjOperation* operation, size_t size,
                 void (*free)(DaiObjStruct* st));
void
DaiObjStruct_Free(DaiVM* vm, DaiObjStruct* obj);

const char*
dai_object_ts(DaiValue value);

static inline bool
dai_is_obj_type(DaiValue value, DaiObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}
#endif /* SRC_DAI_OBJECT_H_ */
