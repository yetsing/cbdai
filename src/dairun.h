#ifndef SRC_DAIRUN_H_
#define SRC_DAIRUN_H_

#include "dai_object.h"
#include "dai_vm.h"

int
Dairun_String(DaiVM* vm, const char* text, const char* filename, DaiObjModule* module);
int
Dairun_File(DaiVM* vm, const char* filename, DaiObjModule* module);

#endif   // SRC_DAIRUN_H_