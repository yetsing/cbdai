#include "daicmd.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <SDL3/SDL.h>

#include "atstr/atstr.h"
#include "dai_compile.h"
#include "dai_debug.h"
#include "dai_error.h"
#include "dai_fmt.h"
#include "dai_malloc.h"
#include "dai_object.h"
#include "dai_parse.h"
#include "dai_utils.h"
#include "dai_vm.h"
#include "dai_windows.h"   // IWYU pragma: keep
#include "dairun.h"
#include "daistd.h"


__attribute__((unused)) static void
sljust(char* dst, char* s, int n) {
    int l = strlen(s);
    memcpy(dst, s, l + 1);
    if (l < n) {
        char* p = dst + l;
        memset(p, ' ', n - l);
        p[n - l] = '\0';
    }
}

int
daicmd_repl() {
    char line[2048] = {0};

    const char* help = "Commands:\n"
                       "  ;time <code> - measure the time of running <code>\n"
                       "  ;help        - show this help\n"
                       "\n"
                       "  Ctrl-d       - exit the repl\n";
    printf("Welcome to Dai repl\n");
    printf("%s", help);

    printf("> ");
    // 初始化
    DaiVM vm;
    DaiVM_init(&vm);
    DaiObjMap* globals;
    DaiObjError* map_err = DaiObjMap_New(&vm, NULL, 0, &globals);
    if (map_err != NULL) {
        DaiVM_printError2(&vm, map_err, "");
        DaiVM_resetStack(&vm);
        DaiVM_reset(&vm);
        return 1;
    }
    // 时间记录
    TimeRecord start_time, end_time;
    bool enable_time = false;
    // 循环读取输入行
    // 这里 n - 1 是为了留一个位置自动填充 ';' ，允许末尾不加 ';' 也能执行
    while (fgets(line, sizeof(line) - 1, stdin) != NULL) {
        vm.state = VMState_pending;
        DaiVM_resetStack(&vm);
        enable_time = false;
        // 自动在末尾加上 ';'
        size_t len = strlen(line);
        if (len == 1) {
            printf("> ");
            continue;
        }
        if (line[len - 1] != ';' && line[len - 2] != ';') {
            line[len - 1] = ';';
        }
        // 处理命令
        const char* input = line;
        if (line[0] == ';') {
            atstr_t atstr = atstr_new(line + 1);
            ATSTR_SPLITN_WRAPPER(&atstr, 2, result, int n);
            // only command
            if (n == 1) {
                input = result[0].start;
                if (atstr_eq2(result[0], "help")) {
                    printf("%s", help);
                    printf("> ");
                } else {
                    printf("unknown command: %s\n", result[0].start);
                    printf("> ");
                }
                continue;
            }
            // command and parameter
            if (n == 2) {
                input = result[1].start;
                if (atstr_eq2(result[0], "time")) {
                    enable_time = true;
                } else {
                    printf("unknown command: %s\n", result[0].start);
                    printf("> ");
                    continue;
                }
            }
        }
        if (enable_time) {
            pin_time_record(&start_time);
        }
        // 执行代码
        DaiObjError* runtime_err = Daiexec_String(&vm, input, globals);
        if (runtime_err != NULL) {
            DaiVM_printError2(&vm, runtime_err, input);
            DaiVM_resetStack(&vm);
            goto end;
        }
        if (enable_time) {
            pin_time_record(&end_time);
            print_used_time(&start_time, &end_time);
        }

        dai_print_value(DaiVM_lastPopedStackElem(&vm));

    end:
        printf("\n");
        printf("> ");

        // 检测 EOF 退出
        if (feof(stdin)) {
            break;
        }
    }
    DaiVM_reset(&vm);
    printf("\n ʕ•̀ ω • ʔ \n");
    return 0;
}

int
daicmd_ast(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s ast <filename>\n", argv[0]);
        return 1;
    }
    const char* filename = argv[2];
    printf("daicmd_ast: %s\n", filename);
    char* input = dai_string_from_file(filename);
    if (input == NULL) {
        perror("Error: cannot read file");
        return 1;
    }
    DaiAstProgram prog;
    DaiAstProgram_init(&prog);
    DaiAstProgram* program = &prog;
    DaiSyntaxError* err    = dai_parse(input, filename, program);
    if (err != NULL) {
        DaiSyntaxError_pprint(err, input);
        free(input);
        return 1;
    }
    {
        char* s = program->string_fn((DaiAstBase*)program, true);
        printf("%s\n", s);
        free(s);
    }
    free(input);
    return 0;
}

int
daicmd_dis(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Usage: %s dis <filename>\n", argv[0]);
        return 1;
    }
    const char* filename = argv[2];
    char* filepath       = realpath(filename, NULL);
    char* text           = dai_string_from_file(filepath);
    if (text == NULL) {
        dai_free(filepath);
        perror("Error: cannot read file");
        return 1;
    }

    bool has_error = false;
    {
        DaiError* err = NULL;
        DaiAstProgram program;
        DaiAstProgram_init(&program);
        DaiVM vm;
        DaiVM_init(&vm);
        if (!daistd_init(&vm)) {
            fprintf(stderr, "Error: cannot initialize std\n");
            has_error = true;
            goto end;
        }

        err = dai_parse(text, filepath, &program);
        if (err != NULL) {
            DaiSyntaxError_setFilename(err, filepath);
            DaiSyntaxError_pprint(err, text);
            goto end;
        }
        DaiObjModule* module = DaiObjModule_New(&vm, strdup("__main__"), strdup(filepath));
        err                  = dai_compile(&program, module, &vm);
        if (err != NULL) {
            DaiCompileError_pprint(err, text);
            goto end;
        }
        dai_log("Module '%s' in %s:\n", module->name->chars, module->filename->chars);
        dai_log("    max_stack_size=%d, max_local_count=%d\n",
                module->max_stack_size,
                module->max_local_count);
        DaiChunk_disassemble(&module->chunk, module->name->chars, 8);
        DaiValueArray constants = module->chunk.constants;
        for (int i = 0; i < constants.count; i++) {
            DaiObjFunction* func = NULL;
            if (IS_CLOSURE(constants.values[i])) {
                func = AS_CLOSURE(constants.values[i])->function;
            } else if (IS_FUNCTION(constants.values[i])) {
                func = AS_FUNCTION(constants.values[i]);
            }
            if (func != NULL) {
                dai_log("Function '%s' in %s:\n", func->name->chars, func->module->filename->chars);
                dai_log("    max_stack_size=%d, max_local_count=%d\n",
                        module->max_stack_size,
                        module->max_local_count);
                DaiChunk_disassemble(&func->chunk, func->name->chars, 8);
            }
        }

    end:
        dai_free(filepath);
        dai_free(text);
        if (err != NULL) {
            DaiError_free(err);
            has_error = true;
        }
        DaiAstProgram_reset(&program);
        DaiVM_reset(&vm);
    }
    return 1 ? has_error : 0;
}

int
daicmd_fmt(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Usage: %s fmt <filename>\n", argv[0]);
        return 1;
    }
    const char* filename = argv[2];
    char* filepath       = realpath(filename, NULL);
    if (filepath == NULL) {
        perror("Error: cannot read file");
        return 1;
    }
    int ret = dai_fmt_file(filepath, true);
    free(filepath);
    return ret;
}

int
daicmd_fmt_check(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Usage: %s fmt_check <filename>\n", argv[0]);
        return 1;
    }
    const char* filename = argv[2];
    char* filepath       = realpath(filename, NULL);
    if (filepath == NULL) {
        perror("Error: cannot read file");
        return 1;
    }
    char* text = dai_string_from_file(filepath);
    if (text == NULL) {
        free(filepath);
        perror("Error: cannot read file");
        return 1;
    }
    int ret = 0;
    DaiAstProgram src_prog;
    DaiAstProgram_init(&src_prog);
    DaiAstProgram dst_prog;
    DaiAstProgram_init(&dst_prog);
    DaiSyntaxError* err = dai_parse(text, filename, &src_prog);
    if (err != NULL) {
        DaiSyntaxError_pprint(err, text);
        ret = 1;
        goto END;
    }

    char* formatted = dai_fmt(&src_prog, strlen(text));
    err             = dai_parse(formatted, filename, &dst_prog);
    if (err != NULL) {
        DaiSyntaxError_pprint(err, formatted);
        ret = 1;
        goto END;
    }

    if (!dai_ast_program_eq(&src_prog, &dst_prog)) {
        printf("fmt_check failed\n");
    }
    free(formatted);

END:
    DaiAstProgram_reset(&src_prog);
    DaiAstProgram_reset(&dst_prog);
    free(filepath);
    free(text);
    return ret;
}

int
daicmd_runfile(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <filename>\n", argv[0]);
        return 1;
    }
    const char* filename = argv[1];
    char* filepath       = realpath(filename, NULL);
    if (filepath == NULL) {
        perror("Error: cannot read file");
        return 1;
    }
    DaiObjError* err = NULL;
    DaiVM vm;
    DaiVM_init(&vm);
    if (!daistd_init(&vm)) {
        fprintf(stderr, "Error: cannot initialize std\n");
        goto END;
    }
    err = Dairun_File(&vm, filepath);
    if (err != NULL) {
        DaiVM_printError(&vm, err);
    }
END:
    free(filepath);
    daistd_quit(&vm);
    DaiVM_reset(&vm);
    return err == NULL ? 0 : 1;
}

int
Daicmd_Main(int argc, char* argv[]) {
    if (argc == 1) {
        return daicmd_repl();
    }
    const char* cmd = argv[1];
    if (strcmp(cmd, "ast") == 0) {
        return daicmd_ast(argc, argv);
    }
    if (strcmp(cmd, "dis") == 0) {
        return daicmd_dis(argc, argv);
    }
    if (strcmp(cmd, "fmt") == 0) {
        return daicmd_fmt(argc, argv);
    }
    if (strcmp(cmd, "fmt_check") == 0) {
        return daicmd_fmt_check(argc, argv);
    }
    return daicmd_runfile(argc, argv);
}