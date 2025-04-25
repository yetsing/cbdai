#ifndef SRC_DAIRUN_H_
#define SRC_DAIRUN_H_

#include "dai_object.h"
#include "dai_vm.h"

DaiObjError*
Dairun_FileWithModule(DaiVM* vm, const char* filename, DaiObjModule* module);

DaiObjError*
Dairun_File(DaiVM* vm, const char* filename);

// 在指定全局环境下执行代码
// 成功返回 NULL，失败返回错误对象
// 执行成功会更新 globals
DaiObjError*
Daiexec_String(DaiVM* vm, const char* text, DaiObjMap* globals);

#endif   // SRC_DAIRUN_H_