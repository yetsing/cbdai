#include "dairun.h"

#include <stdio.h>
#include <stdlib.h>

#include "dai_compile.h"
#include "dai_parse.h"
#include "dai_tokenize.h"
#include "dai_utils.h"
#include "dai_vm.h"


int
Dairun_String(DaiVM* vm, const char* text, const char* filename) {
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
            goto end;
        }
        err = dai_parse(&tlist, &program);
        if (err != NULL) {
            DaiSyntaxError_setFilename(err, filename);
            DaiSyntaxError_pprint(err, text);
            goto end;
        }
        DaiObjFunction* function = DaiObjFunction_New(vm, "<main>", filename);
        err                      = dai_compile(&program, function, vm);
        if (err != NULL) {
            DaiCompileError_pprint(err, text);
            goto end;
        }
        DaiObjError* runtime_err = DaiVM_run(vm, function);
        if (runtime_err != NULL) {
            DaiVM_printError(vm, runtime_err);
            DaiVM_resetStack(vm);
            goto end;
        }

        assert(DaiVM_isEmptyStack(vm));
        // dai_print_value(DaiVM_lastPopedStackElem(&vm));

    end:
        if (err != NULL) {
            DaiError_free(err);
            has_error = true;
        }
        DaiTokenList_reset(&tlist);
        DaiAstProgram_reset(&program);
    }
    return 1 ? has_error : 0;
}

int
Dairun_File(DaiVM* vm, const char* filename) {
    char* filepath = realpath(filename, NULL);
    char* text     = dai_string_from_file(filepath);
    if (text == NULL) {
        free(filepath);
        perror("open file error");
        return 1;
    }
    int rv = Dairun_String(vm, text, filepath);
    free(text);
    free(filepath);
    return rv;
}