#ifndef SRC_DAIRUN_H_
#define SRC_DAIRUN_H_

#include "dai_object.h"
#include "dai_vm.h"

DaiObjError*
Dairun_FileWithModule(DaiVM* vm, const char* filename, DaiObjModule* module);

DaiObjError*
Dairun_File(DaiVM* vm, const char* filename);

#endif   // SRC_DAIRUN_H_