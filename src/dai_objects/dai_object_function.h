#ifndef CBDAI_DAI_OBJECT_FUNCTION_H
#define CBDAI_DAI_OBJECT_FUNCTION_H

#include "dai_chunk.h"
#include "dai_objects/dai_object_base.h"
#include "dai_objects/dai_object_module.h"

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
    int max_stack_size;
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

extern struct DaiObjOperation builtin_function_operation;

typedef struct {
    DaiObj obj;
    const char* name;
    BuiltinFn function;
} DaiObjBuiltinFunction;

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

#endif /* CBDAI_DAI_OBJECT_FUNCTION_H */
