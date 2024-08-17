#include <stdio.h>

#include "munit/munit.h"

#include "dai_compile.h"
#include "dai_memory.h"
#include "dai_object.h"
#include "dai_parse.h"
#include "dai_tokenize.h"
#include "dai_vm.h"

static void
interpret(DaiVM* vm, const char* input) {
    const char* filename = "<test>";
    DaiError*   err;
    // 词法分析
    DaiTokenList* tlist = DaiTokenList_New();
    err                 = dai_tokenize_string(input, tlist);
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
    DaiObjFunction* function = DaiObjFunction_New(vm, "<test>");
    err                      = dai_compile(&program, function, vm);
    if (err) {
        DaiCompileError_pprint(err, input);
    }
    munit_assert_null(err);
    // 运行
    err = DaiVM_run(vm, function);
    if (err) {
        DaiRuntimeError_pprint(err, input);
        munit_assert_null(err);
    }
}

typedef struct {
    const char* input;
    DaiValue    expected;
} DaiVMTestCase;

void
dai_assert_value_equal(DaiValue actual, DaiValue expected);

static void
run_vm_tests(DaiVMTestCase* tests, size_t count) {
    for (int i = 0; i < count; i++) {
        DaiVM vm;
        DaiVM_init(&vm);
        interpret(&vm, tests[i].input);
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

static MunitResult
test_integer_arithmetic(__attribute__((unused)) const MunitParameter params[],
                        __attribute__((unused)) void*                user_data) {
    DaiVMTestCase tests[] = {{"1;", INTEGER_VAL(1)},
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
                             {"(5 + 10 * 2 + 15 / 3) * 2 + -10;", INTEGER_VAL(50)}};
    run_vm_tests(tests, sizeof(tests) / sizeof(tests[0]));
    return MUNIT_OK;
}

static MunitResult
test_boolean_expressions(__attribute__((unused)) const MunitParameter params[],
                         __attribute__((unused)) void*                user_data) {
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
        {"(1 > 2) == true;", dai_false},
        {"(1 > 2) == false;", dai_true},
        {"!true;", dai_false},
        {"!false;", dai_true},
        {"!5;", dai_false},
        {"!!true;", dai_true},
        {"!!false;", dai_false},
        {"!!5;", dai_true},
    };
    run_vm_tests(tests, sizeof(tests) / sizeof(tests[0]));
    return MUNIT_OK;
}

static MunitResult
test_conditionals(__attribute__((unused)) const MunitParameter params[],
                  __attribute__((unused)) void*                user_data) {
    DaiVMTestCase tests[] = {
        {"if (true) { 10; };", INTEGER_VAL(10)},
        {"if (true) { 10; } else { 20 ;};", INTEGER_VAL(10)},
        {"if (false) { 10; } else { 20 ;};", INTEGER_VAL(20)},
        {"if (1) { 10; };", INTEGER_VAL(10)},
        {"if (1 < 2) { 10; };", INTEGER_VAL(10)},
        {"if (1 > 2) { 10; } else { 20; };", INTEGER_VAL(20)},
        {"if (1 < 2) { 10; } else { 20; };", INTEGER_VAL(10)},
        {
            "var x = 1;"
            "if (x == 1) {"
            "  1;"
            "}"
            "elif (x == 2) {"
            "  2;"
            "}"
            "elif (x == 3) {"
            "  3;"
            "}"
            "elif (x == 4) {"
            "  4;"
            "}"
            "elif (x == 5) {"
            "  5;"
            "}"
            "elif (x == 6) {"
            "  6;"
            "}"
            "else {"
            "  999;"
            "}"
            ";"
            "",
            INTEGER_VAL(1),
        },
        {
            "var x = 2;"
            "if (x == 1) {"
            "  1;"
            "}"
            "elif (x == 2) {"
            "  2;"
            "}"
            "elif (x == 3) {"
            "  3;"
            "}"
            "elif (x == 4) {"
            "  4;"
            "}"
            "elif (x == 5) {"
            "  5;"
            "}"
            "elif (x == 6) {"
            "  6;"
            "}"
            "else {"
            "  999;"
            "}"
            ";"
            "",
            INTEGER_VAL(2),
        },
        {
            "var x = 3;"
            "if (x == 1) {"
            "  1;"
            "}"
            "elif (x == 2) {"
            "  2;"
            "}"
            "elif (x == 3) {"
            "  3;"
            "}"
            "elif (x == 4) {"
            "  4;"
            "}"
            "elif (x == 5) {"
            "  5;"
            "}"
            "elif (x == 6) {"
            "  6;"
            "}"
            "else {"
            "  999;"
            "}"
            ";"
            "",
            INTEGER_VAL(3),
        },
        {
            "var x = 4;"
            "if (x == 1) {"
            "  1;"
            "}"
            "elif (x == 2) {"
            "  2;"
            "}"
            "elif (x == 3) {"
            "  3;"
            "}"
            "elif (x == 4) {"
            "  4;"
            "}"
            "elif (x == 5) {"
            "  5;"
            "}"
            "elif (x == 6) {"
            "  6;"
            "}"
            "else {"
            "  999;"
            "}"
            ";"
            "",
            INTEGER_VAL(4),
        },
        {
            "var x = 5;"
            "if (x == 1) {"
            "  1;"
            "}"
            "elif (x == 2) {"
            "  2;"
            "}"
            "elif (x == 3) {"
            "  3;"
            "}"
            "elif (x == 4) {"
            "  4;"
            "}"
            "elif (x == 5) {"
            "  5;"
            "}"
            "elif (x == 6) {"
            "  6;"
            "}"
            "else {"
            "  999;"
            "}"
            ";"
            "",
            INTEGER_VAL(5),
        },
        {
            "var x = 6;"
            "if (x == 1) {"
            "  1;"
            "}"
            "elif (x == 2) {"
            "  2;"
            "}"
            "elif (x == 3) {"
            "  3;"
            "}"
            "elif (x == 4) {"
            "  4;"
            "}"
            "elif (x == 5) {"
            "  5;"
            "}"
            "elif (x == 6) {"
            "  6;"
            "}"
            "else {"
            "  999;"
            "}"
            ";"
            "",
            INTEGER_VAL(6),
        },
        {
            "var x = 111;"
            "if (x == 1) {"
            "  1;"
            "}"
            "elif (x == 2) {"
            "  2;"
            "}"
            "elif (x == 3) {"
            "  3;"
            "}"
            "elif (x == 4) {"
            "  4;"
            "}"
            "elif (x == 5) {"
            "  5;"
            "}"
            "elif (x == 6) {"
            "  6;"
            "}"
            "else {"
            "  999;"
            "}"
            ";"
            "",
            INTEGER_VAL(999),
        },
    };
    run_vm_tests(tests, sizeof(tests) / sizeof(tests[0]));
    return MUNIT_OK;
}

static MunitResult
test_global_var_statements(__attribute__((unused)) const MunitParameter params[],
                           __attribute__((unused)) void*                user_data) {
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
                        __attribute__((unused)) void*                user_data) {
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
                                         __attribute__((unused)) void*                user_data) {
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
                                     __attribute__((unused)) void*                user_data) {
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
                                                   const MunitParameter          params[],
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
                       __attribute__((unused)) void*                user_data) {
    DaiVMTestCase tests[] = {
        {"len(\"\");", INTEGER_VAL(0)},
        {"len(\"four\");", INTEGER_VAL(4)},
        {"len(\"hello\");", INTEGER_VAL(5)},
        {"print(\"hello\");", NIL_VAL},
    };
    run_vm_tests(tests, sizeof(tests) / sizeof(tests[0]));
    return MUNIT_OK;
}

static MunitResult
test_closures(__attribute__((unused)) const MunitParameter params[],
              __attribute__((unused)) void*                user_data) {
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
                    __attribute__((unused)) void*                user_data) {
    DaiVMTestCase tests[] = {
        {
            "class Foo {\n"
            "  fn get() {\n"
            "    return 1;\n"
            "  };\n"
            "};\n"
            "var foo = Foo();\n"
            "foo.get();\n",
            INTEGER_VAL(1),
        },
        {
            "class Foo {\n"
            "  fn get() {\n"
            "    return 1;\n"
            "  };\n"
            "};\n"
            "var foo = Foo();\n"
            "var get =foo.get;\n"
            "get();\n",
            INTEGER_VAL(1),
        },
        {
            "class Foo {\n"
            "  var a = 1;\n"
            "  fn get() {\n"
            "    return 1;\n"
            "  };\n"
            "};\n"
            "var foo = Foo();\n"
            "foo.a;\n",
            INTEGER_VAL(1),
        },
        {
            "class Foo {\n"
            "  var a = 1;\n"
            "  fn get() {\n"
            "    return 1;\n"
            "  };\n"
            "};\n"
            "var foo = Foo();\n"
            "foo.a = 2;\n"
            "foo.a;\n",
            INTEGER_VAL(2),
        },
        {
            "class Foo {\n"
            "  var a = 1;\n"
            "  fn get() {\n"
            "    return self.a;\n"
            "  };\n"
            "};\n"
            "var foo = Foo();\n"
            "foo.get();\n",
            INTEGER_VAL(1),
        },
        {
            "class Foo {\n"
            "  var a = 1;\n"
            "  fn get() {\n"
            "    var ins = self;\n"
            "    return ins.a;\n"
            "  };\n"
            "};\n"
            "var foo = Foo();\n"
            "foo.get();\n",
            INTEGER_VAL(1),
        },
        {
            "class Foo {\n"
            "  var a = 1;\n"
            "  fn get() {\n"
            "    var ins = self;\n"
            "    ins.a = 2;\n"
            "    return ins.a;\n"
            "  };\n"
            "};\n"
            "var foo = Foo();\n"
            "foo.get();\n",
            INTEGER_VAL(2),
        },
        {
            "class Foo {\n"
            "  var a = 1;\n"
            "  fn inc() {\n"
            "    self.a = self.a + 1;\n"
            "  };\n"
            "  fn get() {\n"
            "    var ins = self;\n"
            "    self.inc();"
            "    return ins.a;\n"
            "  };\n"
            "};\n"
            "var foo = Foo(2);\n"
            "foo.get();\n",
            INTEGER_VAL(3),
        },
        {
            "class Foo {\n"
            "  var a = 1;\n"
            "  var increment;"
            "  fn inc() {\n"
            "    self.a = self.a + self.increment;\n"
            "  };\n"
            "  fn get() {\n"
            "    var ins = self;\n"
            "    self.inc();"
            "    return ins.a;\n"
            "  };\n"
            "};\n"
            "var foo = Foo(2, 2);\n"
            "foo.get();\n",
            INTEGER_VAL(4),
        },
        {
            "class Foo {\n"
            "  var a = 1;\n"
            "  var increment;"
            "  fn inc() {\n"
            "    self.a = self.a + self.increment;\n"
            "  };\n"
            "  fn get() {\n"
            "    var ins = self;\n"
            "    self.inc();"
            "    return ins.a;\n"
            "  };\n"
            "  class var c = 2;\n"
            "};\n"
            "var foo = Foo(2, 2);\n"
            "foo.get() + Foo.c;\n",
            INTEGER_VAL(4 + 2),
        },
        {
            "class Foo {\n"
            "  var a = 1;\n"
            "  var increment;"
            "  fn inc() {\n"
            "    self.a = self.a + self.increment;\n"
            "  };\n"
            "  fn get() {\n"
            "    var ins = self;\n"
            "    self.inc();"
            "    return ins.a;\n"
            "  };\n"
            "  class var c = 3;\n"
            "  class fn cget() {\n"
            "     return self.c;\n"
            "  };\n"
            "};\n"
            "var foo = Foo(2, 2);\n"
            "foo.get() + Foo.cget();\n",
            INTEGER_VAL(4 + 3),
        },
        {
            "class Foo {\n"
            "  var a = 1;\n"
            "  var increment;"
            "  fn inc() {\n"
            "    self.a = self.a + self.increment;\n"
            "  };\n"
            "  fn get() {\n"
            "    var ins = self;\n"
            "    self.inc();"
            "    return ins.a;\n"
            "  };\n"
            "  class var c = 0;\n"
            "  class fn cinc() {\n"
            "    self.c = self.c + 1;\n"
            "  };\n"
            "  class fn cget() {\n"
            "     return self.c;\n"
            "  };\n"
            "};\n"
            "Foo.cinc();"
            "var foo = Foo(2, 2);\n"
            "Foo.cinc();"
            "foo.get() + Foo.cget();\n",
            INTEGER_VAL(4 + 2),
        },
        {
            "class Foo {\n"
            "  var a = 1;\n"
            "  var increment;"
            "  fn init(a) {\n"
            "    self.a = a;\n"
            "    self.increment = 2;\n"
            "  };\n"
            "  fn inc() {\n"
            "    self.a = self.a + self.increment;\n"
            "  };\n"
            "  fn get() {\n"
            "    var ins = self;\n"
            "    self.inc();"
            "    return ins.a;\n"
            "  };\n"
            "  class var c = 0;\n"
            "  class fn cinc() {\n"
            "    self.c = self.c + 1;\n"
            "  };\n"
            "  class fn cget() {\n"
            "     return self.c;\n"
            "  };\n"
            "};\n"
            "Foo.cinc();"
            "var foo = Foo(2);\n"
            "Foo.cinc();"
            "foo.get() + Foo.cget();\n",
            INTEGER_VAL(4 + 2),
        },
        {
            "class Foo {\n"
            "  var a = 1;\n"
            "  var increment;"
            "  fn init(a) {\n"
            "    self.a = a;\n"
            "    self.increment = 2;\n"
            "  };\n"
            "  fn inc() {\n"
            "    self.a = self.a + self.increment;\n"
            "  };\n"
            "  fn get() {\n"
            "    var ins = self;\n"
            "    self.inc();"
            "    return ins.a;\n"
            "  };\n"
            "  class var c = 0;\n"
            "  class fn cinc() {\n"
            "    self.c = self.c + 1;\n"
            "  };\n"
            "  class fn cget() {\n"
            "     return self.c;\n"
            "  };\n"
            "};\n"
            "class Goo < Foo {};\n"
            "Goo.cinc();"
            "var goo = Goo(2);\n"
            "Goo.cinc();"
            "goo.get() + Goo.cget();\n",
            INTEGER_VAL(4 + 2),
        },
        {
            "class Foo {\n"
            "  var a = 1;\n"
            "  var increment;"
            "  fn init(a) {\n"
            "    self.a = a;\n"
            "    self.increment = 2;\n"
            "  };\n"
            "  fn inc() {\n"
            "    self.a = self.a + self.increment;\n"
            "  };\n"
            "  fn get() {\n"
            "    var ins = self;\n"
            "    self.inc();"
            "    return ins.a;\n"
            "  };\n"
            "  class var c = 0;\n"
            "  class fn cinc() {\n"
            "    self.c = self.c + 1;\n"
            "  };\n"
            "  class fn cget() {\n"
            "     return self.c;\n"
            "  };\n"
            "};\n"
            "class Goo < Foo {\n"
            "  fn get() {\n"
            "    return super.get();\n"
            "  };\n"
            "  class fn cget() {\n"
            "     return super.cget();\n"
            "  };\n"
            "};\n"
            "Goo.cinc();"
            "var goo = Goo(2);\n"
            "Goo.cinc();"
            "goo.get() + Goo.cget();\n",
            INTEGER_VAL(4 + 2),
        },
    };
    run_vm_tests(tests, sizeof(tests) / sizeof(tests[0]));
    return MUNIT_OK;
}

static MunitResult
test_fibonacci(__attribute__((unused)) const MunitParameter params[],
               __attribute__((unused)) void*                user_data) {
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

MunitTest vm_tests[] = {
    {(char*)"/test_integer_arithmetic",
     test_integer_arithmetic,
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
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};
