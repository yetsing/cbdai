#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <time.h>

#include "atstr/atstr.h"

#include "dai_compile.h"
#include "dai_parse.h"
#include "dai_tokenize.h"
#include "dai_utils.h"
#include "dai_vm.h"

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
main() {
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
    printf("hello world\n");
    return 0;
}
