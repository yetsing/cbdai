#ifndef CBDAI_DAI_OBJECT_STRUCT_H
#define CBDAI_DAI_OBJECT_STRUCT_H

#include "dai_objects/dai_object_base.h"
#include "dai_objects/dai_object_error.h"

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

typedef struct SimpleObjectStruct DaiSimpleObjectStruct;
// 需要检查返回值是否是 NULL
DaiSimpleObjectStruct*
DaiSimpleObjectStruct_New(DaiVM* vm, const char* name, bool readonly);
DaiObjError*
DaiSimpleObjectStruct_add(DaiVM* vm, DaiSimpleObjectStruct* obj, const char* name, DaiValue value);

#endif /* CBDAI_DAI_OBJECT_STRUCT_H */
