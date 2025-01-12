#include <stdio.h>

#include "dai_value.h"
#include "munit/munit.h"

#include "dai_compile.h"
#include "dai_malloc.h"
#include "dai_memory.h"
#include "dai_object.h"
#include "dai_parse.h"
#include "dai_tokenize.h"
#include "dai_utils.h"
#include "dai_vm.h"

#include <dai_debug.h>
#include <linux/limits.h>

// 获取当前文件所在文件夹路径
static void
get_file_directory(char* path) {
    if (realpath(__FILE__, path) == NULL) {
        perror("realpath");
        assert(false);
    }
    char* last_slash = strrchr(path, '/');
    if (last_slash) {
        *(last_slash + 1) = '\0';
    }
}

static DaiObjError*
interpret(DaiVM* vm, const char* input) {
    const char* filename = "<test>";
    // 词法分析
    DaiTokenList* tlist = DaiTokenList_New();
    DaiError* err       = dai_tokenize_string(input, tlist);
    if (err) {
        DaiSyntaxError_setFilename(err, filename);
        DaiSyntaxError_pprint(err, input);
    }
    munit_assert_null(err);
    // 语法分析
    DaiAstProgram program;
    DaiAstProgram_init(&program);
    err = dai_parse(tlist, &program);
    DaiTokenList_free(tlist);
    if (err) {
        DaiSyntaxError_setFilename(err, filename);
        DaiSyntaxError_pprint(err, input);
    }
    munit_assert_null(err);
    // 编译
    DaiObjFunction* function = DaiObjFunction_New(vm, "<test>", "<test>");
    err                      = dai_compile(&program, function, vm);
    if (err) {
        DaiCompileError_pprint(err, input);
    }
    munit_assert_null(err);
    // 运行
    return DaiVM_run(vm, function);
}

static void
interpret2(DaiVM* vm, const char* input) {
    const char* filename = "<test>";
    // 词法分析
    DaiTokenList* tlist = DaiTokenList_New();
    DaiError* err       = dai_tokenize_string(input, tlist);
    if (err) {
        DaiSyntaxError_setFilename(err, filename);
        DaiSyntaxError_pprint(err, input);
    }
    munit_assert_null(err);
    // 语法分析
    DaiAstProgram program;
    DaiAstProgram_init(&program);
    err = dai_parse(tlist, &program);
    DaiTokenList_free(tlist);
    if (err) {
        DaiSyntaxError_setFilename(err, filename);
        DaiSyntaxError_pprint(err, input);
    }
    munit_assert_null(err);
    // 编译
    DaiObjFunction* function = DaiObjFunction_New(vm, "<test>", "<test>");
    err                      = dai_compile(&program, function, vm);
    if (err) {
        DaiCompileError_pprint(err, input);
    }
    munit_assert_null(err);
    // 运行
    DaiObjError* runtime_err = DaiVM_run(vm, function);
    if (runtime_err) {
        DaiChunk_disassemble(&function->chunk, "<test>");
        DaiVM_printError2(vm, runtime_err, input);
        munit_assert_null(err);
    }
}

typedef struct {
    const char* input;
    DaiValue expected;
} DaiVMTestCase;

void
dai_assert_value_equal(DaiValue actual, DaiValue expected);

static void
run_vm_tests(const DaiVMTestCase* tests, const size_t count) {
    for (int i = 0; i < count; i++) {
        DaiVM vm;
        DaiVM_init(&vm);
        if (IS_ERROR(tests[i].expected)) {
            DaiObjError* got_err = interpret(&vm, tests[i].input);
            munit_assert_not_null(got_err);
            dai_assert_value_equal(OBJ_VAL(got_err), tests[i].expected);
        } else {
            interpret2(&vm, tests[i].input);
            DaiValue actual = DaiVM_lastPopedStackElem(&vm);
            dai_assert_value_equal(actual, tests[i].expected);
            if (!DaiVM_isEmptyStack(&vm)) {
                for (DaiValue* slot = vm.stack; slot < vm.stack_top; slot++) {
                    printf("[ ");
                    dai_print_value(*slot);
                    printf(" ]");
                    printf("\n");
                }
            }
            munit_assert_true(DaiVM_isEmptyStack(&vm));
        }
        // 检查垃圾回收
        {
            collectGarbage(&vm);
            size_t before = vm.bytesAllocated;
            collectGarbage(&vm);
            size_t after = vm.bytesAllocated;
            munit_assert_size(before, ==, after);
#ifdef TEST
            test_mark(&vm);
            // 检查所有的对象都被标记了
            DaiObj* obj = vm.objects;
            while (obj != NULL) {
                if (!obj->is_marked) {
                    printf("unmarked object ");
                    dai_print_value(OBJ_VAL(obj));
                    printf("\n");
                }
                munit_assert_true(obj->is_marked);
                obj = obj->next;
            }
#endif
        }

        DaiVM_reset(&vm);
    }
}

static void
run_vm_test_file_with_number(const char* filename) {
    char* input = dai_string_from_file(filename);
    if (input == NULL) {
        printf("could not open file: %s\n", filename);
    }
    munit_assert_not_null(input);
    assert(input != NULL);
    DaiValue expected = INTEGER_VAL(0);
    {
        char* end     = input + 1;
        bool is_float = false;
        while (*end != '\n') {
            if (*end == '.') {
                is_float = true;
            }
            end++;
        }
        char* result   = strndup(input + 1, end - input - 1);
        const double v = strtod(result, &end);
        if (is_float) {
            expected = FLOAT_VAL(v);
        } else {
            expected = INTEGER_VAL((int64_t)v);
        }
        free(result);
    }

    DaiVM vm;
    DaiVM_init(&vm);
    interpret(&vm, input);
    const DaiValue actual = DaiVM_lastPopedStackElem(&vm);
    dai_assert_value_equal(actual, expected);
    if (!DaiVM_isEmptyStack(&vm)) {
        for (const DaiValue* slot = vm.stack; slot < vm.stack_top; slot++) {
            printf("[ ");
            dai_print_value(*slot);
            printf(" ]");
            printf("\n");
        }
    }
    munit_assert_true(DaiVM_isEmptyStack(&vm));
    // 检查垃圾回收
    {
        collectGarbage(&vm);
        const size_t before = vm.bytesAllocated;
        collectGarbage(&vm);
        const size_t after = vm.bytesAllocated;
        munit_assert_size(before, ==, after);
#ifdef TEST
        test_mark(&vm);
        // 检查所有的对象都被标记了
        DaiObj* obj = vm.objects;
        while (obj != NULL) {
            if (!obj->is_marked) {
                printf("unmarked object ");
                dai_print_value(OBJ_VAL(obj));
                printf("\n");
            }
            munit_assert_true(obj->is_marked);
            obj = obj->next;
        }
#endif
    }

    DaiVM_reset(&vm);
    dai_free(input);
}

static void
run_vm_test_files(const char* test_files[], const size_t count) {
    for (int i = 0; i < count; i++) {
        char resolved_path[PATH_MAX];
        get_file_directory(resolved_path);
        strcat(resolved_path, test_files[i]);
        run_vm_test_file_with_number(resolved_path);
    }
}

static MunitResult
test_number_arithmetic(__attribute__((unused)) const MunitParameter params[],
                       __attribute__((unused)) void* user_data) {
    DaiVMTestCase tests[] = {
        {"1;", INTEGER_VAL(1)},
        {"2;", INTEGER_VAL(2)},
        {"1 + 2;", INTEGER_VAL(3)},
        {"1 - 2;", INTEGER_VAL(-1)},
        {"1 * 2;", INTEGER_VAL(2)},
        {"4 / 2;", INTEGER_VAL(2)},
        {"50 / 2 * 2 + 10 - 5;", INTEGER_VAL(55)},
        {"5 + 5 + 5 + 5 - 10;", INTEGER_VAL(10)},
        {"2 * 2 * 2 * 2 * 2;", INTEGER_VAL(32)},
        {"5 * 2 + 10;", INTEGER_VAL(20)},
        {"5 + 2 * 10;", INTEGER_VAL(25)},
        {"5 * (2 + 10);", INTEGER_VAL(60)},
        {"-5;", INTEGER_VAL(-5)},
        {"--5;", INTEGER_VAL(5)},
        {"-10;", INTEGER_VAL(-10)},
        {"-50 + 100 + -50;", INTEGER_VAL(0)},
        {"(5 + 10 * 2 + 15 / 3) * 2 + -10;", INTEGER_VAL(50)},
        {"3.4;", FLOAT_VAL(3.4)},
        {"1 + 3.4;", FLOAT_VAL(4.4)},
        {"1.4 + 3;", FLOAT_VAL(4.4)},
        {"1.4 + 3.0;", FLOAT_VAL(4.4)},
        {"3.4 - 1;", FLOAT_VAL(2.4)},
        {"3 - 0.6;", FLOAT_VAL(2.4)},
        {"3.4 - 1.0;", FLOAT_VAL(2.4)},
        {"3.4 * 1;", FLOAT_VAL(3.4)},
        {"3 * 0.6;", FLOAT_VAL(1.8)},
        {"3.4 * 1.0;", FLOAT_VAL(3.4)},
        {"3.4 / 1;", FLOAT_VAL(3.4)},
        {"3 / 0.6;", FLOAT_VAL(5)},
        {"3.4 / 1.0;", FLOAT_VAL(3.4)},
        {"2 % 1;", INTEGER_VAL(0)},
        {"1 % 2;", INTEGER_VAL(1)},
        {"1 << 2;", INTEGER_VAL(4)},
        {"1 << 2 - 1;", INTEGER_VAL(2)},
        {"1 << (2 - 1);", INTEGER_VAL(2)},
        {"8 >> 2;", INTEGER_VAL(2)},
        {"0b1010 & 0b1100;", INTEGER_VAL(8)},
        {"10 & 12;", INTEGER_VAL(8)},
        {"10 & 24 / 2;", INTEGER_VAL(8)},
        {"10 & 2 * 6;", INTEGER_VAL(8)},
        {"10 & 14 - 2;", INTEGER_VAL(8)},
        {"0b1010 ^ 0b1100;", INTEGER_VAL(6)},
        {"10 ^ 12;", INTEGER_VAL(6)},
        {"10 ^ 10 + 2;", INTEGER_VAL(6)},
        {"0b1010 | 0b1100;", INTEGER_VAL(14)},
        {"10 | 12;", INTEGER_VAL(14)},
        {"1 & 2 ^ 3 | 4;", INTEGER_VAL(7)},
        {"4 | 3 ^ 2 & 1;", INTEGER_VAL(7)},
        {"1 ^ 2 & 3 | 4;", INTEGER_VAL(7)},
        {"1 | 2 ^ 3 & 4;", INTEGER_VAL(3)},
        {"~127;", INTEGER_VAL(-128)},
    };
    run_vm_tests(tests, sizeof(tests) / sizeof(tests[0]));
    return MUNIT_OK;
}

static MunitResult
test_boolean_expressions(__attribute__((unused)) const MunitParameter params[],
                         __attribute__((unused)) void* user_data) {
    DaiVMTestCase tests[] = {
        {"true;", dai_true},
        {"false;", dai_false},
        {"1 < 2;", dai_true},
        {"1 > 2;", dai_false},
        {"1 < 1;", dai_false},
        {"1 > 1;", dai_false},
        {"1 == 1;", dai_true},
        {"1 != 1;", dai_false},
        {"true == true;", dai_true},
        {"false == false;", dai_true},
        {"true == false;", dai_false},
        {"true != false;", dai_true},
        {"false != true;", dai_true},
        {"(1 < 2) == true;", dai_true},
        {"(1 < 2) == false;", dai_false},
        {"(1 <= 2) == true;", dai_true},
        {"(2 <= 2) == true;", dai_true},
        {"(1 > 2) == true;", dai_false},
        {"(1 > 2) == false;", dai_true},
        {"(2 >= 2) == true;", dai_true},
        {"(1 >= 2) == false;", dai_true},
        {"nil == nil;", dai_true},
        {"nil == true;", dai_false},
        {"!true;", dai_false},
        {"!false;", dai_true},
        {"!5;", dai_false},
        {"!!true;", dai_true},
        {"!!false;", dai_false},
        {"!!5;", dai_true},
        {"!nil;", dai_true},
        {"not nil;", dai_true},
        {"not false;", dai_true},
        {"not true;", dai_false},
        {"not 1;", dai_false},
        {"true and true;", dai_true},
        {"true and false;", dai_false},
        {"false and true;", dai_false},
        {"false and false;", dai_false},
        {"true or true;", dai_true},
        {"true or false;", dai_true},
        {"false or true;", dai_true},
        {"false or false;", dai_false},
        // 测试 and or 运算结果
        {"true and 1;", INTEGER_VAL(1)},
        {"0 and 1;", INTEGER_VAL(0)},
        {"false and 1;", dai_false},
        {"1 or false;", INTEGER_VAL(1)},
        {"1 or true;", INTEGER_VAL(1)},
        {"1.0 or true;", FLOAT_VAL(1.0)},
        {"0 or true;", dai_true},
        {"0 or 2;", INTEGER_VAL(2)},
        {"0 or 2.0;", FLOAT_VAL(2.0)},
    };
    run_vm_tests(tests, sizeof(tests) / sizeof(tests[0]));
    return MUNIT_OK;
}

static MunitResult
test_conditionals(__attribute__((unused)) const MunitParameter params[],
                  __attribute__((unused)) void* user_data) {
    const DaiVMTestCase tests[] = {
        {"if (true) { 10; };", INTEGER_VAL(10)},
        {"if (true) { 10; } else { 20 ;};", INTEGER_VAL(10)},
        {"if (false) { 10; } else { 20 ;};", INTEGER_VAL(20)},
        {"if (1) { 10; };", INTEGER_VAL(10)},
        {"if (1 < 2) { 10; };", INTEGER_VAL(10)},
        {"if (1 > 2) { 10; } else { 20; };", INTEGER_VAL(20)},
        {"if (1 < 2) { 10; } else { 20; };", INTEGER_VAL(10)},
    };
    run_vm_tests(tests, sizeof(tests) / sizeof(tests[0]));

    const char* test_files[] = {
        "codes/test_vm/test_conditionals_1.dai",
        "codes/test_vm/test_conditionals_2.dai",
        "codes/test_vm/test_conditionals_3.dai",
        "codes/test_vm/test_conditionals_4.dai",
        "codes/test_vm/test_conditionals_5.dai",
        "codes/test_vm/test_conditionals_6.dai",
        "codes/test_vm/test_conditionals_7.dai",
    };
    run_vm_test_files(test_files, sizeof(test_files) / sizeof(test_files[0]));
    return MUNIT_OK;
}

static MunitResult
test_while_statements(__attribute__((unused)) const MunitParameter params[],
                      __attribute__((unused)) void* user_data) {
    DaiVMTestCase tests[] = {
        {"var a = 1; while (a < 10) { a = a + 1; }; a;", INTEGER_VAL(10)},
        {"var a = 1; while (a < 10) { if (a < 5) { break; }; a = a + 1; }; a;", INTEGER_VAL(1)},
        {
            "var a = 1;\n"
            "while (a < 10) {\n"
            "  if (a < 5) {\n"
            "    break;\n"
            "  };\n"
            "  a = a + 1;\n"
            "}\n"
            "a;",
            INTEGER_VAL(1),
        },
        {
            "var a = 1;\n"
            "while (a < 10) {\n"
            "  if (a > 5) {\n"
            "    break;\n"
            "  };\n"
            "  a = a + 1;\n"
            "}\n"
            "a;",
            INTEGER_VAL(6),
        },
        {
            "var a = 1;\n"
            "var i = 0;\n"
            "while (i < 10) {\n"
            "  i = i + 1;\n"
            "  if (i > 4) {\n"
            "    continue;\n"
            "  };\n"
            "  a = a + 1;\n"
            "}\n"
            "a;",
            INTEGER_VAL(5),
        }};
    run_vm_tests(tests, sizeof(tests) / sizeof(tests[0]));
    return MUNIT_OK;
}


static MunitResult
test_global_var_statements(__attribute__((unused)) const MunitParameter params[],
                           __attribute__((unused)) void* user_data) {
    DaiVMTestCase tests[] = {
        {"var one = 1; one;", INTEGER_VAL(1)},
        {"var one = 1; one = 2; one;", INTEGER_VAL(2)},
        {"var one = 1; var two = 2; one + two;", INTEGER_VAL(3)},
        {"var one = 1; var two = 2; var three = one + two; three;", INTEGER_VAL(3)},
    };
    run_vm_tests(tests, sizeof(tests) / sizeof(tests[0]));
    return MUNIT_OK;
}

static MunitResult
test_string_expressions(__attribute__((unused)) const MunitParameter params[],
                        __attribute__((unused)) void* user_data) {
    DaiObjString monkey =
        (DaiObjString){{.type = DaiObjType_string}, .chars = "monkey", .length = 6};
    DaiObjString monkeybanana =
        (DaiObjString){{.type = DaiObjType_string}, .chars = "monkeybanana", .length = 11};
    DaiVMTestCase tests[] = {
        {
            "\"monkey\";",
            OBJ_VAL(&monkey),
        },
        {
            "\"mon\" + \"key\";",
            OBJ_VAL(&monkey),
        },
        {
            "\"mon\" + \"key\" + \"banana\";",
            OBJ_VAL(&monkeybanana),
        },
    };
    run_vm_tests(tests, sizeof(tests) / sizeof(tests[0]));
    return MUNIT_OK;
}

static MunitResult
test_calling_functions_without_arguments(__attribute__((unused)) const MunitParameter params[],
                                         __attribute__((unused)) void* user_data) {
    DaiVMTestCase tests[] = {
        {
            "var fivePlusTen = fn() {return 5 + 10; };\n fivePlusTen();",
            INTEGER_VAL(15),
        },
        {
            "var one = fn() { return 1; };\n var two = fn() { return 2; };\n one() + two();",
            INTEGER_VAL(3),
        },
        {
            "var a = fn() {return 1;};\n var b = fn() {return a() + 1;};\n var c = fn() {return "
            "b() + 1;};\n c();",
            INTEGER_VAL(3),
        },
        {
            "var earlyExit = fn() {return 99; 100;};\n earlyExit();",
            INTEGER_VAL(99),
        },
        {
            "var earlyExit = fn() {return 99; return 100;};\n earlyExit();",
            INTEGER_VAL(99),
        },
    };
    run_vm_tests(tests, sizeof(tests) / sizeof(tests[0]));
    return MUNIT_OK;
}

static MunitResult
test_calling_functions_without_return_value(__attribute__((unused)) const MunitParameter params[],
                                            __attribute__((unused)) void* user_data) {
    DaiVMTestCase tests[] = {
        {
            "var noReturn = fn() { };\n noReturn();",
            NIL_VAL,
        },
        {
            "var noReturn = fn() {};\n var noReturn2 = fn() {noReturn();};\n noReturn();\n "
            "noReturn2();",
            NIL_VAL,
        },
    };
    run_vm_tests(tests, sizeof(tests) / sizeof(tests[0]));
    return MUNIT_OK;
}

static MunitResult
test_calling_functions_with_bindings(__attribute__((unused)) const MunitParameter params[],
                                     __attribute__((unused)) void* user_data) {
    DaiVMTestCase tests[] = {
        {
            "var one = fn() {var one = 1; return one;};\n one();",
            INTEGER_VAL(1),
        },
        {
            "var oneAndTwo = fn() {var one = 1; var two = 2; return one + two;};\n oneAndTwo();",
            INTEGER_VAL(3),
        },
        {
            "var oneAndTwo = fn() {var one = 1; var two = 2; return one + two;};\n var "
            "threeAndFour = fn() {var three = 3; var four = 4; return three + four;};\n "
            "oneAndTwo() + threeAndFour();",
            INTEGER_VAL(10),
        },
        {
            "var firstFoobar = fn() {var foobar = 50; return foobar;};\n var secondFoobar = fn() "
            "{var foobar = 100; return foobar;};\n firstFoobar() + secondFoobar();",
            INTEGER_VAL(150),
        },
        {
            "var globalSeed = 50;\n"
            "var minusOne = fn() {var num = 1; return globalSeed - num;};\n"
            "var minusTwo = fn() {var num = 2; return globalSeed - num;};\n"
            "minusOne() + minusTwo();",
            INTEGER_VAL(97),
        },
        {
            "var one = fn() {\n"
            "  var a = 1;\n"
            "  if (2 > 1) {\n"
            "    var a = 2;\n"
            "    return a;\n"
            "  };\n"
            "};\n"
            "one();",
            INTEGER_VAL(2),
        },
        {
            "var one = fn() {\n"
            "  var a = 1;\n"
            "  if (2 > 1) {\n"
            "    var a = 2;\n"
            "  };\n"
            "  return a;\n"
            "};\n"
            "one();",
            INTEGER_VAL(1),
        },
    };
    run_vm_tests(tests, sizeof(tests) / sizeof(tests[0]));
    return MUNIT_OK;
}

static MunitResult
test_calling_functions_with_arguments_and_bindings(__attribute__((unused))
                                                   const MunitParameter params[],
                                                   __attribute__((unused)) void* user_data) {
    DaiVMTestCase tests[] = {
        {
            "var identity = fn(a) {return a;};\n identity(4);",
            INTEGER_VAL(4),
        },
        {
            "var sum = fn(a, b) {return a + b;};\n sum(1, 2);",
            INTEGER_VAL(3),
        },
        {
            "var sum = fn(a, b) { var c = a + b; return c;};\n sum(1, 2);",
            INTEGER_VAL(3),
        },
        {
            "var sum = fn(a, b) { var c = a + b; return c;};\n sum(1, 2) + sum(3, 4);",
            INTEGER_VAL(10),
        },
        {
            "var sum = fn(a, b) {var c = a + b; return c;};\n var outer = fn() {return sum(1, 2) + "
            "sum(3, 4);};\n outer();",
            INTEGER_VAL(10),
        },
        {
            "var globalNum = 10;\n var sum = fn(a, b) { var c = a + b; return c + globalNum;};\n "
            "var outer = fn() {return sum(1, 2) + sum(3, 4) + globalNum;};\n outer() + globalNum;",
            INTEGER_VAL(50),
        },
    };
    run_vm_tests(tests, sizeof(tests) / sizeof(tests[0]));
    return MUNIT_OK;
}

static MunitResult
test_builtin_functions(__attribute__((unused)) const MunitParameter params[],
                       __attribute__((unused)) void* user_data) {
    DaiVMTestCase tests[] = {
        {"len(\"\");", INTEGER_VAL(0)},
        {"len(\"four\");", INTEGER_VAL(4)},
        {"len(\"hello\");", INTEGER_VAL(5)},
        {"len([]);", INTEGER_VAL(0)},
        {"len([1]);", INTEGER_VAL(1)},
        {"var m = [1, 2, 3]; len(m);", INTEGER_VAL(3)},
        {"print(\"hello\");", NIL_VAL},
    };
    run_vm_tests(tests, sizeof(tests) / sizeof(tests[0]));
    return MUNIT_OK;
}

static MunitResult
test_closures(__attribute__((unused)) const MunitParameter params[],
              __attribute__((unused)) void* user_data) {
    DaiVMTestCase tests[] = {
        {
            "var newClosure = fn(a) {\n"
            "  return fn() {return a;};\n"
            "};\n"
            "var closure = newClosure(99);\n"
            "closure();",
            INTEGER_VAL(99),
        },
        {
            "var newAdder = fn(a, b) {\n"
            "    return fn(c) {return a + b + c;};\n"
            "};"
            "var adder = newAdder(1, 2);\n"
            "adder(8);",
            INTEGER_VAL(11),
        },
        {
            "var newAdder = fn(a, b) {\n"
            "    var c = a + b;\n"
            "    return fn(d) { return c + d; };\n"
            "};\n"
            "var adder = newAdder(1, 2);\n"
            "adder(8);",
            INTEGER_VAL(11),
        },
        {
            "var newAdderOuter = fn(a, b) {\n"
            "    var c = a + b;\n"
            "    return fn(d) {\n"
            "        var e = c + d;\n"
            "        return fn(f) { return e + f; };\n"
            "    };\n"
            "};\n"
            "var newAdderInner = newAdderOuter(1, 2);\n"
            "var adder = newAdderInner(3);\n"
            "adder(8);",
            INTEGER_VAL(14),
        },
        {
            "var a = 1;\n"
            "var newAdderOuter = fn(b) {\n"
            "    return fn(c) {\n"
            "        return fn(d) { return a + b + c + d; };\n"
            "    };\n"
            "};\n"
            "var newAdderInner = newAdderOuter(2);\n"
            "var adder = newAdderInner(3);\n"
            "adder(8);",
            INTEGER_VAL(14),
        },
        {
            "var newClosure = fn(a, b) {\n"
            "    var one = fn() { return a; };\n"
            "    var two = fn() { return b; };\n"
            "    return fn() { return one() + two(); };\n"
            "};\n"
            "var closure = newClosure(9, 90);\n"
            "closure();",
            INTEGER_VAL(99),
        },
    };
    run_vm_tests(tests, sizeof(tests) / sizeof(tests[0]));
    return MUNIT_OK;
}

static MunitResult
test_class_instance(__attribute__((unused)) const MunitParameter params[],
                    __attribute__((unused)) void* user_data) {
    const char* test_files[] = {
        "codes/test_vm/test_class_instance_1.dai",
        "codes/test_vm/test_class_instance_2.dai",
        "codes/test_vm/test_class_instance_3.dai",
        "codes/test_vm/test_class_instance_4.dai",
        "codes/test_vm/test_class_instance_5.dai",
        "codes/test_vm/test_class_instance_6.dai",
        "codes/test_vm/test_class_instance_7.dai",
        "codes/test_vm/test_class_instance_8.dai",
        "codes/test_vm/test_class_instance_9.dai",
        "codes/test_vm/test_class_instance_10.dai",
        "codes/test_vm/test_class_instance_11.dai",
        "codes/test_vm/test_class_instance_12.dai",
        "codes/test_vm/test_class_instance_13.dai",
        "codes/test_vm/test_class_instance_14.dai",
        "codes/test_vm/test_class_instance_15.dai",
        "codes/test_vm/test_class_instance_16.dai",
    };
    run_vm_test_files(test_files, sizeof(test_files) / sizeof(test_files[0]));
    return MUNIT_OK;
}

static MunitResult
test_fibonacci(__attribute__((unused)) const MunitParameter params[],
               __attribute__((unused)) void* user_data) {
    DaiVMTestCase tests[] = {
        {
            "var fib = fn(n) {if (n < 2) { return n; }; return fib(n - 1) + fib(n - 2);};\n"
            "fib(1);",
            INTEGER_VAL(1),
        },
        {
            "var fib = fn(n) {if (n < 2) { return n; }; return fib(n - 1) + fib(n - 2);};\n"
            "fib(2);",
            INTEGER_VAL(1),
        },
        {
            "var fib = fn(n) {if (n < 2) { return n; }; return fib(n - 1) + fib(n - 2);};\n"
            "fib(3);",
            INTEGER_VAL(2),
        },
        {
            "var fib = fn(n) {if (n < 2) { return n; }; return fib(n - 1) + fib(n - 2);};\n"
            "fib(10);",
            INTEGER_VAL(55),
        },

    };
    run_vm_tests(tests, sizeof(tests) / sizeof(tests[0]));
    return MUNIT_OK;
}

__attribute__((unused)) static MunitResult
test_array(__attribute__((unused)) const MunitParameter params[],
           __attribute__((unused)) void* user_data) {
    const DaiVMTestCase tests[] = {
        {
            "[0, 1, 2][0];",
            INTEGER_VAL(0),
        },
        {
            "[0, 1, 2][1];",
            INTEGER_VAL(1),
        },
        {
            "[0, 1, 2][2];",
            INTEGER_VAL(2),
        },
        {
            "[0, 1, 1 + 1][2];",
            INTEGER_VAL(2),
        },
        {
            "[0, 1, 2][1 - 1];",
            INTEGER_VAL(0),
        },
        {
            "var m = [0, 1, 2]; m[1 - 1];",
            INTEGER_VAL(0),
        },
        {
            "var m = [0, 1, 2]; m[- 1];",
            INTEGER_VAL(2),
        },
        {
            "var m = [0 + 0 - 0, 1, 2]; m[- 3];",
            INTEGER_VAL(0),
        },

    };
    run_vm_tests(tests, sizeof(tests) / sizeof(tests[0]));
    return MUNIT_OK;
}

__attribute__((unused)) static MunitResult
test_array_method(__attribute__((unused)) const MunitParameter params[],
                  __attribute__((unused)) void* user_data) {
    const DaiVMTestCase tests[] = {
        // length
        {
            "var m = []; m.length();",
            INTEGER_VAL(0),
        },
        {
            "var m = [0]; m.length();",
            INTEGER_VAL(1),
        },
        {
            "var m = [0, 1]; m.length();",
            INTEGER_VAL(2),
        },
        {
            "var m = [0, 1, 2]; m.length();",
            INTEGER_VAL(3),
        },

        // add
        {
            "var m = []; m.add(0); m.length();",
            INTEGER_VAL(1),
        },
        {
            "var m = []; m.add(0, 1); m.length();",
            INTEGER_VAL(2),
        },
        {
            "var m = []; m.add(0, 1, 2); m.length();",
            INTEGER_VAL(3),
        },
        {
            "var m = []; m.add(0, 1, 2); m[-1];",
            INTEGER_VAL(2),
        },

        // pop
        {
            "var m = [1, 2, 3, 4]; m.pop();",
            INTEGER_VAL(4),
        },
        {
            "var m = [1, 2, 3, 4]; m.pop(); m.pop();",
            INTEGER_VAL(3),
        },
        {
            "var m = [1, 2, 3, 4]; m.pop(); m.pop(); m.pop();",
            INTEGER_VAL(2),
        },

        // sub
        {
            "var m = [1, 2, 3, 4]; var n = m.sub(0, 13); n.length();",
            INTEGER_VAL(4),
        },
        {
            "var m = [1, 2, 3, 4]; var n = m.sub(-4, 13); n.length();",
            INTEGER_VAL(4),
        },
        {
            "var m = [1, 2, 3, 4]; var n = m.sub(-40, 13); n.length();",
            INTEGER_VAL(4),
        },
        {
            "var m = [1, 2, 3, 4]; var n = m.sub(-40, -13); n.length();",
            INTEGER_VAL(0),
        },
        {
            "var m = [1, 2, 3, 4]; var n = m.sub(-40, -1); n.length();",
            INTEGER_VAL(3),
        },
        {
            "var m = [1, 2, 3, 4]; var n = m.sub(-40, -1); n[-1];",
            INTEGER_VAL(3),
        },
        {
            "var m = [1, 2, 3, 4]; var n = m.sub(0, 3); n.length();",
            INTEGER_VAL(3),
        },
        {
            "var m = [1, 2, 3, 4]; var n = m.sub(0, -1); n.length();",
            INTEGER_VAL(3),
        },
        {
            "var m = [1, 2, 3, 4]; var n = m.sub(0, 3); n[0];",
            INTEGER_VAL(1),
        },
        {
            "var m = [1, 2, 3, 4]; var n = m.sub(0, 3); n[1];",
            INTEGER_VAL(2),
        },
        {
            "var m = [1, 2, 3, 4]; var n = m.sub(0, 3); n[2];",
            INTEGER_VAL(3),
        },
        {
            "var m = [1, 2, 3, 4]; var n = m.sub(3); n.length();",
            INTEGER_VAL(1),
        },
        {
            "var m = [1, 2, 3, 4]; var n = m.sub(3); n[0];",
            INTEGER_VAL(4),
        },

        // remove
        {
            "var m = [1, 2, 3, 4]; m.remove(1); m.length();",
            INTEGER_VAL(3),
        },
        {
            "var m = [1, 2, 3, 4]; m.remove(1); m[0];",
            INTEGER_VAL(2),
        },
        {
            "var m = [1, 2, 3, 4]; m.remove(2); m.length();",
            INTEGER_VAL(3),
        },
        {
            "var m = [1, 2, 3, 4]; m.remove(2); m[1];",
            INTEGER_VAL(3),
        },
        {
            "var m = [1, 2, 3, 4]; m.remove(4); m.length();",
            INTEGER_VAL(3),
        },
        {
            "var m = [1, 2, 3, 4]; m.remove(4); m[-1];",
            INTEGER_VAL(3),
        },
        {
            "var m = [1, 2, 3, 4, 4]; m.remove(4); m[-1];",
            INTEGER_VAL(4),
        },
        {
            "var m = [1, 2, 3, '字符串', 4]; m.remove('字符串'); m[3];",
            INTEGER_VAL(4),
        },

        // removeIndex
        {
            "var m = [1, 3, 4, '字符串', 5, 3]; m.removeIndex(4); m[4];",
            INTEGER_VAL(3),
        },
        {
            "var m = [1, 3, 4, '字符串', 5, 3]; m.removeIndex(-1); m[-1];",
            INTEGER_VAL(5),
        },

        // extend
        {
            "var m = [1, 3, 4]; var n = [2, 5, 6]; m.extend(n); m.length();",
            INTEGER_VAL(6),
        },
        {
            "var m = [1, 3, 4]; var n = [2, 5, 6]; m.extend(n); n.length();",
            INTEGER_VAL(3),
        },
        {
            "var m = [1, 3, 4]; var n = [2, 5, 6]; m.extend(n); m[-1];",
            INTEGER_VAL(6),
        },

        // has
        {
            "var m = [1, 2, 'nihk', 3.14]; m.has(1);",
            dai_true,
        },
        {
            "var m = [1, 2, 'nihk', 3.14]; m.has(3);",
            dai_false,
        },
        {
            "var m = [1, 2, 'nihk', 3.14]; m.has('nihk');",
            dai_true,
        },
        {
            "var m = [1, 2, 'nihk', 3.14]; m.has(3.14);",
            dai_true,
        },

        // reversed return new array
        {
            "var m = [1, 2, 3, 4]; var n = m.reversed(); n.length();",
            INTEGER_VAL(4),
        },
        {
            "var m = [1, 2, 3, 4]; var n = m.reversed(); n[0];",
            INTEGER_VAL(4),
        },
        {
            "var m = [1, 2, 3, 4]; var n = m.reversed(); n[1];",
            INTEGER_VAL(3),
        },
        {
            "var m = [1, 2, 3, 4]; var n = m.reversed(); n[2];",
            INTEGER_VAL(2),
        },
        {
            "var m = [1, 2, 3, 4]; var n = m.reversed(); n[3];",
            INTEGER_VAL(1),
        },
        {
            "var m = [1, 2, 3, 4]; var n = m.reversed(); m[0];",
            INTEGER_VAL(1),
        },

        // reverse in place
        {
            "var m = [1, 2, 3, 4]; m.reverse(); m.length();",
            INTEGER_VAL(4),
        },
        {
            "var m = [1, 2, 3, 4]; m.reverse(); m[0];",
            INTEGER_VAL(4),
        },
        {
            "var m = [1, 2, 3, 4]; m.reverse(); m[1];",
            INTEGER_VAL(3),
        },
        {
            "var m = [1, 2, 3, 4]; m.reverse(); m[2];",
            INTEGER_VAL(2),
        },
        {
            "var m = [1, 2, 3, 4]; m.reverse(); m[3];",
            INTEGER_VAL(1),
        },
        {
            "var m = [1, 2, 3, 4]; m.reverse(); m[-1];",
            INTEGER_VAL(1),
        },

        // sort
        {
            "var m = [1, 3, 2, 4]; m.sort(fn(e1, e2) {return e1 - e2;}); m.length();",
            INTEGER_VAL(4),
        },
        {
            "var m = [1, 3, 2, 4]; m.sort(fn(e1, e2) {return e1 - e2;}); m[0];",
            INTEGER_VAL(1),
        },
        {
            "var m = [1, 3, 2, 4]; m.sort(fn(e1, e2) {return e1 - e2;}); m[1];",
            INTEGER_VAL(2),
        },
        {
            "var m = [1, 3, 2, 4]; m.sort(fn(e1, e2) {return e1 - e2;}); m[2];",
            INTEGER_VAL(3),
        },
        {
            "var m = [1, 3, 2, 4]; m.sort(fn(e1, e2) {return e1 - e2;}); m[3];",
            INTEGER_VAL(4),
        },
        {
            "var m = [1, 3, 2, 4]; m.sort(fn(e1, e2) {return e2 - e1;}); m[0];",
            INTEGER_VAL(4),
        },
        {
            "var m = [1, 3, 2, 4]; m.sort(fn(e1, e2) {return e2 - e1;}); m[1];",
            INTEGER_VAL(3),
        },
        {
            "var m = [1, 3, 2, 4]; m.sort(fn(e1, e2) {return e2 - e1;}); m[2];",
            INTEGER_VAL(2),
        },
        {
            "var m = [1, 3, 2, 4]; m.sort(fn(e1, e2) {return e2 - e1;}); m[3];",
            INTEGER_VAL(1),
        },
        {
            "var m = [4, 3, 2, 1]; m.sort(fn(e1, e2) {return e1 - e2;}); m[-1];",
            INTEGER_VAL(4),
        },
        {
            "var m = [1, 2, 3, 4]; m.sort(fn(e1, e2) {return e2 - e1;}); m[-1];",
            INTEGER_VAL(1),
        },
        {
            "var m = [1, 1, 1, 1]; m.sort(fn(e1, e2) {return e2 - e1;}); m[-1];",
            INTEGER_VAL(1),
        },
    };
    run_vm_tests(tests, sizeof(tests) / sizeof(tests[0]));
    return MUNIT_OK;
}

static MunitResult
test_array_subscript_set(__attribute__((unused)) const MunitParameter params[],
                         __attribute__((unused)) void* user_data) {
    const DaiVMTestCase tests[] = {
        {
            "var m = [0, 1, 2]; m[0] = 1; m[0];",
            INTEGER_VAL(1),
        },
        {
            "var m = [0, 1, 2]; m[1] = 2; m[1];",
            INTEGER_VAL(2),
        },
        {
            "var m = [0, 1, 2]; m[2] = 3; m[2];",
            INTEGER_VAL(3),
        },
        {
            "var m = [0, 1, 2]; m[1 + 1] = 3; m[2];",
            INTEGER_VAL(3),
        },
        {
            "var m = [0, 1, 2]; m[1 - 1] = 3; m[0];",
            INTEGER_VAL(3),
        },
        {
            "var m = [0, 1, 2]; m[- 1] = 3; m[2];",
            INTEGER_VAL(3),
        },
        {
            "var m = [0, 1, 2]; m[- 3] = 3; m[0];",
            INTEGER_VAL(3),
        },
    };
    run_vm_tests(tests, sizeof(tests) / sizeof(tests[0]));
    return MUNIT_OK;
}


static MunitResult
test_error(__attribute__((unused)) const MunitParameter params[],
           __attribute__((unused)) void* user_data) {
    DaiVM vm;
    DaiVM_init(&vm);
    const DaiVMTestCase tests[] = {
        {
            "var a = 1 / 0;",
            OBJ_VAL(DaiObjError_Newf(&vm, "division by zero")),
        },
        {
            "var a = 1 / 0.0;",
            OBJ_VAL(DaiObjError_Newf(&vm, "division by zero")),
        },
        {
            "1 + 'string';",
            OBJ_VAL(DaiObjError_Newf(&vm, "unsupported operand type(s) for +: 'int' and 'string'")),
        },
        {
            "1 - 'string';",
            OBJ_VAL(DaiObjError_Newf(&vm, "unsupported operand type(s) for -: 'int' and 'string'")),
        },
        {
            "1 * 'string';",
            OBJ_VAL(DaiObjError_Newf(&vm, "unsupported operand type(s) for *: 'int' and 'string'")),
        },
        {
            "1 / 'string';",
            OBJ_VAL(DaiObjError_Newf(&vm, "unsupported operand type(s) for /: 'int' and 'string'")),
        },
        {
            "1 % 'string';",
            OBJ_VAL(
                DaiObjError_Newf(&vm, "unsupported operand type(s) for %%: 'int' and 'string'")),
        },
        {
            "1 < 'string';",
            OBJ_VAL(
                DaiObjError_Newf(&vm, "unsupported operand type(s) for >/<: 'string' and 'int'")),
        },
        {
            "1 <= 'string';",
            OBJ_VAL(
                DaiObjError_Newf(&vm, "unsupported operand type(s) for >=/<=: 'string' and 'int'")),
        },
        {
            "1 > 'string';",
            OBJ_VAL(
                DaiObjError_Newf(&vm, "unsupported operand type(s) for >/<: 'int' and 'string'")),
        },
        {
            "1 >= 'string';",
            OBJ_VAL(
                DaiObjError_Newf(&vm, "unsupported operand type(s) for >=/<=: 'int' and 'string'")),
        },
        {
            "1 & 'string';",
            OBJ_VAL(DaiObjError_Newf(&vm, "unsupported operand type(s) for &: 'int' and 'string'")),
        },
        {
            "1 | 'string';",
            OBJ_VAL(DaiObjError_Newf(&vm, "unsupported operand type(s) for |: 'int' and 'string'")),
        },
        {
            "~ 'string';",
            OBJ_VAL(DaiObjError_Newf(&vm, "unsupported operand type(s) for ~: 'string'")),
        },
        {
            "1 ^ 'string';",
            OBJ_VAL(DaiObjError_Newf(&vm, "unsupported operand type(s) for ^: 'int' and 'string'")),
        },
        {
            "1 << 'string';",
            OBJ_VAL(
                DaiObjError_Newf(&vm, "unsupported operand type(s) for <<: 'int' and 'string'")),
        },
        {
            "1 >> 'string';",
            OBJ_VAL(
                DaiObjError_Newf(&vm, "unsupported operand type(s) for >>: 'int' and 'string'")),
        },

        {
            "var a = [1, 2, 3]; a[3];",
            OBJ_VAL(DaiObjError_Newf(&vm, "array index out of bounds")),
        },
        {
            "var a = []; a[0] = 1;",
            OBJ_VAL(DaiObjError_Newf(&vm, "array index out of bounds")),
        },
        {
            "var a = []; a.length(1);",
            OBJ_VAL(DaiObjError_Newf(&vm, "length() expected no arguments, but got 1")),
        },
        {
            "var a = []; a.add();",
            OBJ_VAL(DaiObjError_Newf(&vm,
                                     "add() expected one or more arguments, but got no arguments")),
        },
        {
            "var a = []; a.pop(1, 2);",
            OBJ_VAL(DaiObjError_Newf(&vm, "pop() expected no arguments, but got 2")),
        },
        {
            "var a = []; a.pop();",
            OBJ_VAL(DaiObjError_Newf(&vm, "pop from empty array")),
        },
        {
            "var a = []; a.sub();",
            OBJ_VAL(DaiObjError_Newf(&vm, "sub() expected 1-2 arguments, but got 0")),
        },
        {
            "var a = []; a.sub('a');",
            OBJ_VAL(DaiObjError_Newf(&vm, "sub() expected int arguments, but got string")),
        },
        {
            "var a = []; a.sub(1, 'a');",
            OBJ_VAL(DaiObjError_Newf(&vm, "sub() expected int arguments, but got string")),
        },
        {
            "var a = []; a.remove(1, 'a');",
            OBJ_VAL(DaiObjError_Newf(&vm, "remove() expected 1 arguments, but got 2")),
        },
        {
            "var a = []; a.remove(1);",
            OBJ_VAL(DaiObjError_Newf(&vm, "array.remove(x): x not in array")),
        },
        {
            "var a = []; a.removeIndex();",
            OBJ_VAL(DaiObjError_Newf(&vm, "removeIndex() expected 1 arguments, but got 0")),
        },
        {
            "var a = []; a.removeIndex(1);",
            OBJ_VAL(DaiObjError_Newf(&vm, "removeIndex() index out of bounds")),
        },
        {
            "var a = []; a.extend();",
            OBJ_VAL(DaiObjError_Newf(&vm, "extend() expected 1 arguments, but got 0")),
        },
        {
            "var a = []; a.extend(1);",
            OBJ_VAL(DaiObjError_Newf(&vm, "extend() expected array arguments, but got int")),
        },
        {
            "var a = []; a.has();",
            OBJ_VAL(DaiObjError_Newf(&vm, "has() expected 1 arguments, but got 0")),
        },
        {
            "var a = []; a.reversed(1);",
            OBJ_VAL(DaiObjError_Newf(&vm, "reversed() expected 0 arguments, but got 1")),
        },
        {
            "var a = []; a.reverse(1);",
            OBJ_VAL(DaiObjError_Newf(&vm, "reverse() expected 0 arguments, but got 1")),
        },
        {
            "var a = [1, 2]; a.sort(1);",
            OBJ_VAL(DaiObjError_Newf(&vm, "'int' object is not callable")),
        },
        {
            "var a = [1, 2]; a.sort( fn(a, b) {return '';} );",
            OBJ_VAL(DaiObjError_Newf(&vm, "sort cmp() expected int return value, but got string")),
        },

        {
            "len(1);",
            OBJ_VAL(DaiObjError_Newf(&vm, "'len' not supported 'int'")),
        },


        {
            "var a = fn() { return 1 + 'string'; }; a(1);",
            OBJ_VAL(DaiObjError_Newf(&vm, "<anonymous 0>() expected 0 arguments but got 1")),
        },
        {
            "fn recursive() { recursive(); }; recursive();",
            OBJ_VAL(DaiObjError_Newf(&vm, "maximum recursion depth exceeded")),
        },
        {
            "var a = 'string'; a(1);",
            OBJ_VAL(DaiObjError_Newf(&vm, "'string' object is not callable")),
        },


        {
            "var a = 1; a.nonexistent;",
            OBJ_VAL(DaiObjError_Newf(&vm, "'int' object has not property 'nonexistent'")),
        },
        {
            "var a = 1; a.nonexistent = 2;",
            OBJ_VAL(DaiObjError_Newf(&vm, "'int' object has not property 'nonexistent'")),
        },
        {
            "var a = [1]; a.nonexistent;",
            OBJ_VAL(DaiObjError_Newf(&vm, "'array' object has not property 'nonexistent'")),
        },
        {
            "var a = [1]; a.nonexistent = 2;",
            OBJ_VAL(DaiObjError_Newf(&vm, "'array' object has not property 'nonexistent'")),
        },
        {
            "class F { var a = 1; }; var f = F(); f.nonexistent;",
            OBJ_VAL(DaiObjError_Newf(&vm, "'F' object has not property 'nonexistent'")),
        },
        {
            "class F { var a = 1; }; var f = F(); f.a();",
            OBJ_VAL(DaiObjError_Newf(&vm, "'int' object is not callable")),
        },
        {
            "class F { var a = 1; fn get() { return super.a;}; }; var f = F(); f.get();",
            OBJ_VAL(DaiObjError_Newf(&vm, "no superclass found")),
        },
        {
            "class F { var a = 1; fn get() { return self.b;}; }; var f = F(); f.get();",
            OBJ_VAL(DaiObjError_Newf(&vm, "'F' object has not property 'b'")),
        },
        {
            "class F { var a = 1; fn get() { return self.b();}; }; var f = F(); f.get();",
            OBJ_VAL(DaiObjError_Newf(&vm, "'F' object has not property 'b'")),
        },
        {
            "class F { var a = 1; fn set() { self.b = 1;}; }; var f = F(); f.set();",
            OBJ_VAL(DaiObjError_Newf(&vm, "'F' object has not property 'b'")),
        },
        {
            "class F { var a = 1; fn set() { self.b = 1;}; }; var f = F(); f.set(1);",
            OBJ_VAL(DaiObjError_Newf(&vm, "set() expected 0 arguments but got 1")),
        },
        {
            "class G { var a = 1; }; class F < G { var a = 1; fn get() { return super.a;}; }; "
            "var f = F(); f.get();",
            OBJ_VAL(DaiObjError_Newf(&vm, "'super' object has not property 'a'")),
        },

    };
    run_vm_tests(tests, sizeof(tests) / sizeof(tests[0]));
    DaiVM_reset(&vm);
    return MUNIT_OK;
}


MunitTest vm_tests[] = {
    {(char*)"/test_number_arithmetic",
     test_number_arithmetic,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/test_boolean_expressions",
     test_boolean_expressions,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/test_conditionals", test_conditionals, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/test_while_statements",
     test_while_statements,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/test_global_var_statements",
     test_global_var_statements,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/test_string_expressions",
     test_string_expressions,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/test_calling_functions_without_arguments",
     test_calling_functions_without_arguments,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/test_calling_functions_without_return_value",
     test_calling_functions_without_return_value,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/test_calling_functions_with_bindings",
     test_calling_functions_with_bindings,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/test_calling_functions_with_arguments_and_bindings",
     test_calling_functions_with_arguments_and_bindings,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/test_builtin_functions",
     test_builtin_functions,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/test_closures", test_closures, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/test_class_instance", test_class_instance, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/test_fibonacci", test_fibonacci, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/test_array", test_array, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/test_array_method", test_array_method, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/test_array_subscript_set",
     test_array_subscript_set,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/test_error", test_error, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};
