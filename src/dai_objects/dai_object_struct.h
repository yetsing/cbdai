#ifndef CBDAI_DAI_OBJECT_STRUCT_H
#define CBDAI_DAI_OBJECT_STRUCT_H

#include "dai_objects/dai_object_base.h"
#include "dai_objects/dai_object_error.h"

// ref_count/refs 用来引用对象，在垃圾回收时，要标记这些对象
typedef struct DaiObjStruct DaiObjStruct;
#define DAI_OBJ_STRUCT_BASE                      \
    DaiObj obj;                                  \
    const char* name;                            \
    size_t size;                                 \
    void (*free)(DaiVM * vm, DaiObjStruct * st); \
    size_t ref_count;                            \
    DaiValue refs[10];

typedef struct DaiObjStruct {
    DAI_OBJ_STRUCT_BASE
} DaiObjStruct;
// name 使用字符串字面量，DaiObjStruct 不会管理 name 的内存
DaiObjStruct*
DaiObjStruct_New(DaiVM* vm, const char* name, struct DaiObjOperation* operation, size_t size,
                 void (*free)(DaiVM* vm, DaiObjStruct* st));
void
DaiObjStruct_Free(DaiVM* vm, DaiObjStruct* obj);
void
DaiObjStruct_add_ref(DaiVM* vm, DaiObjStruct* obj, DaiValue value);

typedef struct SimpleObjectStruct DaiSimpleObjectStruct;
// 需要检查返回值是否是 NULL
DaiSimpleObjectStruct*
DaiSimpleObjectStruct_New(DaiVM* vm, const char* name, bool readonly);
DaiObjError*
DaiSimpleObjectStruct_add(DaiVM* vm, DaiSimpleObjectStruct* obj, const char* name, DaiValue value);

#endif /* CBDAI_DAI_OBJECT_STRUCT_H */
