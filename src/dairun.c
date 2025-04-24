#include "dairun.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dai_object.h"
#include "dai_utils.h"
#include "dai_vm.h"
#include "dai_windows.h"   // IWYU pragma: keep


DaiObjError*
Dairun_FileWithModule(DaiVM* vm, const char* filename, DaiObjModule* module) {
    char* filepath = realpath(filename, NULL);
    char* text     = dai_string_from_file(filepath);
    if (text == NULL) {
        free(filepath);
        perror("open file error");
        return DaiObjError_Newf(vm, "open file error %s", filename);
    }
    DaiObjError* err = DaiVM_loadModule(vm, text, module);
    free(text);
    free(filepath);
    return err;
}

DaiObjError*
Dairun_File(DaiVM* vm, const char* filename) {
    DaiObjModule* module = DaiObjModule_New(vm, strdup("__main__"), strdup(filename));
    return Dairun_FileWithModule(vm, filename, module);
}