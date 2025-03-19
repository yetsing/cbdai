#include "daicmd.h"

#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <time.h>

#include "atstr/atstr.h"
#include "dai_compile.h"
#include "dai_debug.h"
#include "dai_malloc.h"
#include "dai_parse.h"
#include "dai_tokenize.h"
#include "dai_utils.h"
#include "dai_vm.h"
#include "dairun.h"

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
    const char* filename = "<stdin>";
    char line[2048]      = {0};

    const char* help = "Commands:\n"
                       "  ;time <code> - measure the time of running <code>\n"
                       "  ;help        - show this help\n"
                       "\n"
                       "  Ctrl-d       - exit the repl\n";
    printf("Welcome to Dai repl\n");
    printf("%s", help);

    printf("> ");
    // 初始化
    DaiError* err = NULL;
    DaiTokenList tlist;
    DaiTokenList_init(&tlist);
    DaiAstProgram program;
    DaiAstProgram_init(&program);
    DaiVM vm;
    DaiVM_init(&vm);
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
        err = dai_tokenize_string(input, &tlist);
        if (err != NULL) {
            DaiSyntaxError_setFilename(err, filename);
            DaiSyntaxError_pprint(err, input);
            goto end;
        }
        err = dai_parse(&tlist, &program);
        if (err != NULL) {
            DaiSyntaxError_setFilename(err, filename);
            DaiSyntaxError_pprint(err, input);
            goto end;
        }
        DaiObjFunction* function = DaiObjFunction_New(&vm, "<main>", "<stdin>");
        err                      = dai_compile(&program, function, &vm);
        if (err != NULL) {
            DaiCompileError_pprint(err, input);
            goto end;
        }
        DaiObjError* runtime_err = DaiVM_run(&vm, function);
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
        printf("\n");

    end:
        if (err != NULL) {
            DaiError_free(err);
        }
        DaiTokenList_reset(&tlist);
        DaiAstProgram_reset(&program);
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

static int
parse_helper(const char* input, DaiAstProgram* program) {
    DaiTokenList* tlist = DaiTokenList_New();
    DaiSyntaxError* err = dai_tokenize_string(input, tlist);
    if (err != NULL) {
        DaiSyntaxError_setFilename(err, "<stdin>");
        DaiSyntaxError_pprint(err, input);
        return -1;
    }
    err = dai_parse(tlist, program);
    if (err != NULL) {
        DaiSyntaxError_setFilename(err, "<stdin>");
        DaiSyntaxError_pprint(err, input);
        return -1;
    }
    DaiTokenList_free(tlist);
    return 0;
}

int
daicmd_ast(int argc, const char** argv) {
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
    if (parse_helper(input, program) < 0) {
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
daicmd_dis(int argc, const char** argv) {
    if (argc != 3) {
        printf("Usage: %s dis <filename>\n", argv[0]);
        return 1;
    }
    const char* filename = argv[2];
    char* filepath       = realpath(filename, NULL);
    char* text           = dai_string_from_file(filename);
    if (text == NULL) {
        free(filepath);
        perror("Error: cannot read file");
        return 1;
    }

    bool has_error = false;
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
        DaiChunk_disassemble(&function->chunk, function->name->chars);
        DaiValueArray constants = function->chunk.constants;
        for (int i = 0; i < constants.count; i++) {
            DaiObjFunction* func = NULL;
            if (IS_CLOSURE(constants.values[i])) {
                func = AS_CLOSURE(constants.values[i])->function;
            } else if (IS_FUNCTION(constants.values[i])) {
                func = AS_FUNCTION(constants.values[i]);
            }
            if (func != NULL) {
                DaiChunk_disassemble(&func->chunk, func->name->chars);
            }
        }

    end:
        dai_free(filepath);
        dai_free(text);
        if (err != NULL) {
            DaiError_free(err);
            has_error = true;
        }
        DaiTokenList_reset(&tlist);
        DaiAstProgram_reset(&program);
        DaiVM_reset(&vm);
    }
    return 1 ? has_error : 0;
}


int
daicmd_runfile(int argc, const char** argv) {
    if (argc != 2) {
        printf("Usage: %s <filename>\n", argv[0]);
        return 1;
    }
    const char* filename = argv[1];
    DaiVM vm;
    DaiVM_init(&vm);
    int ret = Dairun_File(&vm, filename);
    DaiVM_reset(&vm);
    return ret;
}

int
Daicmd_Main(int argc, const char** argv) {
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
    return daicmd_runfile(argc, argv);
}