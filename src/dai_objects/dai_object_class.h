#ifndef DAI_OBJECT_CLASS_H
#define DAI_OBJECT_CLASS_H

#include <string.h>

#include "dai_objects/dai_object_base.h"
#include "dai_objects/dai_object_function.h"
#include "dai_objects/dai_object_tuple.h"
#include "dai_table.h"

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

#endif //DAI_OBJECT_CLASS_H
