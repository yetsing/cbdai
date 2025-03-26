#include "dairun.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dai_compile.h"
#include "dai_object.h"
#include "dai_parse.h"
#include "dai_tokenize.h"
#include "dai_utils.h"
#include "dai_vm.h"


int
Dairun_String(DaiVM* vm, const char* text, const char* filename, DaiObjModule* module) {
    assert(text != NULL && filename != NULL);

    bool has_error = false;
    {
        DaiError* err = NULL;
        DaiTokenList tlist;
        DaiTokenList_init(&tlist);
        DaiAstProgram program;
        DaiAstProgram_init(&program);

        err = dai_tokenize_string(text, &tlist);
        if (err != NULL) {
            DaiSyntaxError_setFilename(err, filename);
            DaiSyntaxError_pprint(err, text);
            has_error = true;
            goto end;
        }
        err = dai_parse(&tlist, &program);
        if (err != NULL) {
            DaiSyntaxError_setFilename(err, filename);
            DaiSyntaxError_pprint(err, text);
            has_error = true;
            goto end;
        }
        err = dai_compile(&program, module, vm);
        if (err != NULL) {
            DaiCompileError_pprint(err, text);
            has_error = true;
            goto end;
        }
        DaiObjError* runtime_err = DaiVM_main(vm, module);
        if (runtime_err != NULL) {
            DaiVM_printError(vm, runtime_err);
            DaiVM_resetStack(vm);
            has_error = true;
            goto end;
        }

    end:
        if (err != NULL) {
            DaiError_free(err);
        }
        DaiTokenList_reset(&tlist);
        DaiAstProgram_reset(&program);
    }
    if (!has_error) {
        return 0;
    }
    return 1;
}

int
Dairun_File(DaiVM* vm, const char* filename, DaiObjModule* module) {
    char* filepath = realpath(filename, NULL);
    char* text     = dai_string_from_file(filepath);
    if (text == NULL) {
        free(filepath);
        perror("open file error");
        return 1;
    }
    int ret = Dairun_String(vm, text, filepath, module);
    free(text);
    free(filepath);
    return ret;
}