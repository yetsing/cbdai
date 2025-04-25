#ifndef SRC_DAI_OBJECT_MODULE_H_
#define SRC_DAI_OBJECT_MODULE_H_

#include "hashmap.h"

#include "dai_chunk.h"
#include "dai_objects/dai_object_base.h"
#include "dai_objects/dai_object_map.h"
#include "dai_symboltable.h"

typedef struct {
    DaiObj obj;
    DaiChunk chunk;
    DaiObjString *name, *filename;
    bool compiled;

    // 全局变量和全局符号表
    struct hashmap* global_map;
    DaiValue* globals;

    int max_local_count;
    int max_stack_size;

    DaiVM* vm;
} DaiObjModule;
DaiObjModule*
DaiObjModule_New(DaiVM* vm, const char* name, const char* filename);
DaiObjModule*
DaiObjModule_NewWithGlobals(DaiVM* vm, const char* name, const char* filename, DaiObjMap* globals);
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
DaiObjModule_add_global1(DaiObjModule* module, DaiObjString* name, DaiValue value);
bool
DaiObjModule_iter(DaiObjModule* module, size_t* i, DaiObjString** key, DaiValue* value);
// copy from module.globals to globals
void
DaiObjModule_copyto(DaiObjModule* module, DaiObjMap* globals);
void
builtin_module_setup(DaiObjModule* module);

#endif /* SRC_DAI_OBJECT_MODULE_H_ */