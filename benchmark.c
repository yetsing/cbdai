#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "dai_assert.h"
#include "dai_compile.h"
#include "dai_malloc.h"
#include "dai_parse.h"
#include "dai_tokenize.h"
#include "dai_utils.h"
#include "dai_vm.h"

int
main(int argc, char** argv) {
    for (int i = 0; i < argc; i++) {
        printf("argv[%d] = %s\n", i, argv[i]);
    }
    if (argc != 2) {
        printf("Usage: %s <filename>\n", argv[0]);
        return 1;
    }
    char* filename = argv[1];
    char* filepath = realpath(filename, NULL);
    char* text     = dai_string_from_file(filename);
    daiassert(text != NULL, "Failed to open %s\n", filename);

    // 时间记录
    TimeRecord start_time, end_time;
    pin_time_record(&start_time);
    {
        DaiError* err = NULL;
        DaiTokenList tlist;
        DaiTokenList_init(&tlist);
        DaiAstProgram program;
        DaiAstProgram_init(&program);
        DaiVM vm;
        DaiVM_init(&vm);

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
        DaiObjFunction* function = DaiObjFunction_New(&vm, "<main>", filepath);
        err                      = dai_compile(&program, function, &vm);
        if (err != NULL) {
            DaiCompileError_pprint(err, text);
            goto end;
        }
        DaiObjError* runtime_err = DaiVM_run(&vm, function);
        if (runtime_err != NULL) {
            DaiVM_printError(&vm, runtime_err);
            DaiVM_resetStack(&vm);
            goto end;
        }

        // dai_print_value(DaiVM_lastPopedStackElem(&vm));

    end:
        dai_free(filepath);
        dai_free(text);
        if (err != NULL) {
            DaiError_free(err);
        }
        DaiTokenList_reset(&tlist);
        DaiAstProgram_reset(&program);
        DaiVM_reset(&vm);
    }
    pin_time_record(&end_time);
    printf("\n\n=================== Used Time ====================\n");
    print_used_time(&start_time, &end_time);
}