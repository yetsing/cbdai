#include <stdio.h>

#include "munit/munit.h"

#include "dai_compile.h"
#include "dai_debug.h"
#include "dai_object.h"
#include "dai_parse.h"
#include "dai_tokenize.h"
void
dai_assert_value_equal(DaiValue actual, DaiValue expected);

static void
compile_helper(const char* input, DaiObjFunction* function, DaiVM* vm) {
    DaiError* err;
    // 词法分析
    DaiTokenList* tlist = DaiTokenList_New();
    err                 = dai_tokenize_string(input, tlist);
    if (err) {
        DaiSyntaxError_setFilename(err, "<stdin>");
        DaiSyntaxError_pprint(err, input);
    }
    munit_assert_null(err);
    DaiAstProgram program;
    DaiAstProgram_init(&program);
    err = dai_parse(tlist, &program);
    if (err) {
        DaiSyntaxError_setFilename(err, "<stdin>");
        DaiSyntaxError_pprint(err, input);
    }
    munit_assert_null(err);
    {
        // char *s = program->string_fn((DaiAstBase*)program, true);
        // printf(s);
        // free(s);
    }
    DaiTokenList_free(tlist);
    err = dai_compile(&program, function, vm);
    if (err) {
        DaiCompileError_pprint(err, input);
    }
    munit_assert_null(err);
}

static DaiCompileError*
compile_error_helper(const char* input, DaiObjFunction* function, DaiVM* vm) {
    DaiError* err;
    // 词法分析
    DaiTokenList* tlist = DaiTokenList_New();
    err                 = dai_tokenize_string(input, tlist);
    if (err) {
        DaiSyntaxError_setFilename(err, "<stdin>");
        DaiSyntaxError_pprint(err, input);
    }
    munit_assert_null(err);
    DaiAstProgram program;
    DaiAstProgram_init(&program);
    err = dai_parse(tlist, &program);
    if (err) {
        DaiSyntaxError_setFilename(err, "<stdin>");
        DaiSyntaxError_pprint(err, input);
    }
    munit_assert_null(err);
    DaiTokenList_free(tlist);
    err = dai_compile(&program, function, vm);
    return err;
}

typedef struct {
    const char* input;
    int expected_count;
    uint8_t expected_codes[64];
    DaiValue expected_constants[32];

} DaiCompilerTestCase;

// 将嵌套的常量展开
static void
flat_constants(DaiValueArray* target, const DaiValueArray* source) {
    for (int i = 0; i < source->count; i++) {
        if (IS_FUNCTION(source->values[i])) {
            flat_constants(target, &(AS_FUNCTION(source->values[i])->chunk.constants));
        }
        DaiValueArray_write(target, source->values[i]);
    }
}

static void
run_compiler_tests(const DaiCompilerTestCase* tests, const size_t count) {
    for (size_t i = 0; i < count; i++) {
        DaiVM vm;
        DaiVM_init(&vm);
        DaiObjFunction* function = DaiObjFunction_New(&vm, "<test>");
        compile_helper(tests[i].input, function, &vm);
        DaiChunk chunk = function->chunk;
        if (chunk.count != tests[i].expected_count) {
            DaiChunk_disassemble(&chunk, "test actual");
        }
        munit_assert_int(chunk.count, ==, tests[i].expected_count);
        for (int j = 0; j < chunk.count; j++) {
            if (chunk.code[j] != tests[i].expected_codes[j]) {
                DaiChunk_disassemble(&chunk, "test");
            }
            munit_assert_uint8(chunk.code[j], ==, tests[i].expected_codes[j]);
        }
        // 将函数内嵌套的常量全部展开，方便测试
        DaiValueArray got_constants;
        DaiValueArray_init(&got_constants);
        flat_constants(&got_constants, &chunk.constants);
        for (int j = 0; j < got_constants.count; j++) {
            dai_assert_value_equal(got_constants.values[j], tests[i].expected_constants[j]);
        }
        DaiValueArray_reset(&got_constants);
        DaiVM_reset(&vm);
    }
}

static MunitResult
test_integer_arithmetic(__attribute__((unused)) const MunitParameter params[],
                        __attribute__((unused)) void* user_data) {
    DaiCompilerTestCase tests[] = {
        {
            "1 + 2;",
            8,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpConstant,
                0,
                1,
                DaiOpAdd,
                DaiOpPop,
            },
            {
                INTEGER_VAL(1),
                INTEGER_VAL(2),
            },
        },
        {
            "1; 2;",
            8,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpPop,
                DaiOpConstant,
                0,
                1,
                DaiOpPop,
            },
            {
                INTEGER_VAL(1),
                INTEGER_VAL(2),
            },

        },
        {
            "1 - 2;",
            8,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpConstant,
                0,
                1,
                DaiOpSub,
                DaiOpPop,
            },
            {
                INTEGER_VAL(1),
                INTEGER_VAL(2),
            },
        },
        {
            "1 * 2;",
            8,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpConstant,
                0,
                1,
                DaiOpMul,
                DaiOpPop,
            },
            {
                INTEGER_VAL(1),
                INTEGER_VAL(2),
            },
        },
        {
            "2 / 1;",
            8,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpConstant,
                0,
                1,
                DaiOpDiv,
                DaiOpPop,
            },
            {
                INTEGER_VAL(2),
                INTEGER_VAL(1),
            },
        },
        {
            "2 % 1;",
            8,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpConstant,
                0,
                1,
                DaiOpMod,
                DaiOpPop,
            },
            {
                INTEGER_VAL(2),
                INTEGER_VAL(1),
            },
        },

        {
            "-1;",
            5,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpMinus,
                DaiOpPop,
            },
            {
                INTEGER_VAL(1),
            },
        },
        {
            "-1.0;",
            5,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpMinus,
                DaiOpPop,
            },
            {
                FLOAT_VAL(1.0),
            },
        },
        {
            "3.1415926;",
            4,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpPop,
            },
            {
                FLOAT_VAL(3.1415926),
            },
        },

    };
    run_compiler_tests(tests, sizeof(tests) / sizeof(tests[0]));
    return MUNIT_OK;
}

static MunitResult
test_boolean_expressions(__attribute__((unused)) const MunitParameter params[],
                         __attribute__((unused)) void* user_data) {
    DaiCompilerTestCase tests[] = {
        {
            "true;",
            2,
            {
                DaiOpTrue,
                DaiOpPop,
            },
        },
        {
            "false;",
            2,
            {
                DaiOpFalse,
                DaiOpPop,
            },
        },
        {
            "nil;",
            2,
            {
                DaiOpNil,
                DaiOpPop,
            },
        },

        {
            "1 > 2;",
            8,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpConstant,
                0,
                1,
                DaiOpGreaterThan,
                DaiOpPop,
            },
            {
                INTEGER_VAL(1),
                INTEGER_VAL(2),
            },
        },
        {
            "1 < 2;",
            8,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpConstant,
                0,
                1,
                DaiOpGreaterThan,
                DaiOpPop,
            },
            {
                INTEGER_VAL(2),
                INTEGER_VAL(1),
            },
        },
        {
            "1 == 2;",
            8,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpConstant,
                0,
                1,
                DaiOpEqual,
                DaiOpPop,
            },
            {
                INTEGER_VAL(1),
                INTEGER_VAL(2),
            },
        },
        {
            "1 != 2;",
            8,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpConstant,
                0,
                1,
                DaiOpNotEqual,
                DaiOpPop,
            },
            {
                INTEGER_VAL(1),
                INTEGER_VAL(2),
            },
        },
        {
            "true == false;",
            4,
            {
                DaiOpTrue,
                DaiOpFalse,
                DaiOpEqual,
                DaiOpPop,
            },
        },
        {
            "true != false;",
            4,
            {
                DaiOpTrue,
                DaiOpFalse,
                DaiOpNotEqual,
                DaiOpPop,
            },
        },
        {
            "!true;",
            3,
            {
                DaiOpTrue,
                DaiOpBang,
                DaiOpPop,
            },
        },
        {
            "nil != nil;",
            4,
            {
                DaiOpNil,
                DaiOpNil,
                DaiOpNotEqual,
                DaiOpPop,
            },
        },
        {
            "!nil;",
            3,
            {
                DaiOpNil,
                DaiOpBang,
                DaiOpPop,
            },
        },

    };
    run_compiler_tests(tests, sizeof(tests) / sizeof(tests[0]));
    return MUNIT_OK;
}

static MunitResult
test_conditionals(__attribute__((unused)) const MunitParameter params[],
                  __attribute__((unused)) void* user_data) {
    DaiCompilerTestCase tests[] = {
        {
            "if (true) {10;};\n 3333;",
            12,
            {
                DaiOpTrue,
                DaiOpJumpIfFalse,
                0,
                4,
                DaiOpConstant,
                0,
                0,
                DaiOpPop,
                DaiOpConstant,
                0,
                1,
                DaiOpPop,
            },
            {
                INTEGER_VAL(10),
                INTEGER_VAL(3333),
            },
        },
        {
            "if (true) {10;} \nelse {20;};\n 3333;",
            19,
            {
                DaiOpTrue, DaiOpJumpIfFalse, 0, 7, DaiOpConstant, 0, 0,
                DaiOpPop,  DaiOpJump,        0, 4, DaiOpConstant, 0, 1,
                DaiOpPop,  DaiOpConstant,    0, 2, DaiOpPop,
            },
            {
                INTEGER_VAL(10),
                INTEGER_VAL(20),
                INTEGER_VAL(3333),
            },
        },
        {
            "if (true) {\n"
            "    1;\n"
            "} elif (true) {\n"
            "    2;\n"
            "};\n"
            "",
            19,
            {
                DaiOpTrue, DaiOpJumpIfFalse, 0, 7, DaiOpConstant, 0, 0, DaiOpPop, DaiOpJump, 0, 8,
                DaiOpTrue, DaiOpJumpIfFalse, 0, 4, DaiOpConstant, 0, 1, DaiOpPop,
            },
            {
                INTEGER_VAL(1),
                INTEGER_VAL(2),
            },

        },
        {
            "if (true) {\n"
            "    1;\n"
            "} elif (true) {\n"
            "    2;\n"
            "} else {\n"
            "    3;\n"
            "};\n"
            "",
            26,
            {
                DaiOpTrue,
                DaiOpJumpIfFalse,
                0,
                7,
                DaiOpConstant,
                0,
                0,
                DaiOpPop,
                DaiOpJump,
                0,
                15,

                DaiOpTrue,
                DaiOpJumpIfFalse,
                0,
                7,
                DaiOpConstant,
                0,
                1,
                DaiOpPop,
                DaiOpJump,
                0,
                4,

                DaiOpConstant,
                0,
                2,
                DaiOpPop,
            },
            {
                INTEGER_VAL(1),
                INTEGER_VAL(2),
                INTEGER_VAL(3),
            },

        },
        {
            "if (true) {\n"
            "    1;\n"
            "} elif (true) {\n"
            "    2;\n"
            "} elif (true) {\n"
            "    3;\n"
            "} else {\n"
            "    9;\n"
            "};\n"
            "",
            37,
            {
                DaiOpTrue,
                DaiOpJumpIfFalse,
                0,
                7,
                DaiOpConstant,
                0,
                0,
                DaiOpPop,
                DaiOpJump,
                0,
                26,

                DaiOpTrue,
                DaiOpJumpIfFalse,
                0,
                7,
                DaiOpConstant,
                0,
                1,
                DaiOpPop,
                DaiOpJump,
                0,
                15,

                DaiOpTrue,
                DaiOpJumpIfFalse,
                0,
                7,
                DaiOpConstant,
                0,
                2,
                DaiOpPop,
                DaiOpJump,
                0,
                4,

                DaiOpConstant,
                0,
                3,
                DaiOpPop,
            },
            {
                INTEGER_VAL(1),
                INTEGER_VAL(2),
                INTEGER_VAL(3),
                INTEGER_VAL(9),
            },

        },
    };
    run_compiler_tests(tests, sizeof(tests) / sizeof(tests[0]));
    return MUNIT_OK;
}

static MunitResult
test_while_statements(__attribute__((unused)) const MunitParameter params[],
                      __attribute__((unused)) void* user_data) {
    DaiCompilerTestCase tests[] = {
        {
            "while (false) { }",
            7,
            {
                DaiOpFalse,
                DaiOpJumpIfFalse,
                0,
                3,
                DaiOpJumpBack,
                0,
                7,
            },
            {},
        },
        {
            "while (false) { 1; }",
            11,
            {
                DaiOpFalse,
                DaiOpJumpIfFalse,
                0,
                7,
                DaiOpConstant,
                0,
                0,
                DaiOpPop,
                DaiOpJumpBack,
                0,
                11,
            },
            {
                INTEGER_VAL(1),
            },
        },
        {
            "while (false) { 1; break; }",
            14,
            {
                DaiOpFalse,
                DaiOpJumpIfFalse,
                0,
                10,
                DaiOpConstant,
                0,
                0,
                DaiOpPop,
                DaiOpJump,
                0,
                3,
                DaiOpJumpBack,
                0,
                14,
            },
            {
                INTEGER_VAL(1),
            },
        },
        {
            "while (false) { 1; continue; }",
            14,
            {
                DaiOpFalse,
                DaiOpJumpIfFalse,
                0,
                10,
                DaiOpConstant,
                0,
                0,
                DaiOpPop,
                DaiOpJumpBack,
                0,
                11,
                DaiOpJumpBack,
                0,
                14,
            },
            {
                INTEGER_VAL(1),
            },
        },
        {
            "while (false) { break; 1; break; }",
            17,
            {
                DaiOpFalse,
                DaiOpJumpIfFalse,
                0,
                13,
                DaiOpJump,
                0,
                10,
                DaiOpConstant,
                0,
                0,
                DaiOpPop,
                DaiOpJump,
                0,
                3,
                DaiOpJumpBack,
                0,
                17,
            },
            {
                INTEGER_VAL(1),
            },
        },
        {
            "while (false) { continue; 1; continue; }",
            17,
            {
                DaiOpFalse,
                DaiOpJumpIfFalse,
                0,
                13,
                DaiOpJumpBack,
                0,
                7,
                DaiOpConstant,
                0,
                0,
                DaiOpPop,
                DaiOpJumpBack,
                0,
                14,
                DaiOpJumpBack,
                0,
                17,
            },
            {
                INTEGER_VAL(1),
            },
        },
        {
            "while (true) { while (false) { break; 1; break; } break; }",
            27,
            {
                DaiOpTrue,
                DaiOpJumpIfFalse,
                0,
                23,

                DaiOpFalse,
                DaiOpJumpIfFalse,
                0,
                13,
                DaiOpJump,
                0,
                10,
                DaiOpConstant,
                0,
                0,
                DaiOpPop,
                DaiOpJump,
                0,
                3,
                DaiOpJumpBack,
                0,
                17,

                DaiOpJump,
                0,
                3,
                DaiOpJumpBack,
                0,
                27,
            },
            {
                INTEGER_VAL(1),
            },
        },
        {
            "while (true) {\n"
            "  break;\n"
            "  while (false) {\n"
            "    break;"
            "    1;"
            "    break;\n"
            "  }"
            "  break;\n"
            "}"
            "",
            30,
            {
                DaiOpTrue,
                DaiOpJumpIfFalse,
                0,
                26,
                DaiOpJump,
                0,
                23,

                DaiOpFalse,
                DaiOpJumpIfFalse,
                0,
                13,
                DaiOpJump,
                0,
                10,
                DaiOpConstant,
                0,
                0,
                DaiOpPop,
                DaiOpJump,
                0,
                3,
                DaiOpJumpBack,
                0,
                17,

                DaiOpJump,
                0,
                3,
                DaiOpJumpBack,
                0,
                30,
            },
            {
                INTEGER_VAL(1),
            },
        },
        {
            "while (true) {\n"
            "  break;\n"
            "  continue;\n"
            "  while (false) {\n"
            "    break;"
            "    1;"
            "    break;\n"
            "  }"
            "  break;\n"
            "}"
            "",
            33,
            {
                DaiOpTrue,
                DaiOpJumpIfFalse,
                0,
                29,
                DaiOpJump,
                0,
                26,
                DaiOpJumpBack,
                0,
                10,

                DaiOpFalse,
                DaiOpJumpIfFalse,
                0,
                13,
                DaiOpJump,
                0,
                10,
                DaiOpConstant,
                0,
                0,
                DaiOpPop,
                DaiOpJump,
                0,
                3,
                DaiOpJumpBack,
                0,
                17,

                DaiOpJump,
                0,
                3,
                DaiOpJumpBack,
                0,
                33,
            },
            {
                INTEGER_VAL(1),
            },
        },
        {
            "while (true) {\n"
            "  break;\n"
            "  continue;\n"
            "  while (false) {\n"
            "    continue;\n"
            "    break;\n"
            "    1;\n"
            "    break;\n"
            "  }"
            "  break;\n"
            "}"
            "",
            36,
            {
                DaiOpTrue,
                DaiOpJumpIfFalse,
                0,
                32,
                DaiOpJump,
                0,
                29,
                DaiOpJumpBack,
                0,
                10,

                DaiOpFalse,
                DaiOpJumpIfFalse,
                0,
                16,
                DaiOpJumpBack,
                0,
                7,
                DaiOpJump,
                0,
                10,
                DaiOpConstant,
                0,
                0,
                DaiOpPop,
                DaiOpJump,
                0,
                3,
                DaiOpJumpBack,
                0,
                20,

                DaiOpJump,
                0,
                3,
                DaiOpJumpBack,
                0,
                36,
            },
            {
                INTEGER_VAL(1),
            },
        },
    };
    run_compiler_tests(tests, sizeof(tests) / sizeof(tests[0]));
    return MUNIT_OK;
}

static MunitResult
test_global_var_statements(__attribute__((unused)) const MunitParameter params[],
                           __attribute__((unused)) void* user_data) {
    DaiCompilerTestCase tests[] = {
        {
            "var one = 1;\n var two = 2;",
            12,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0,
                DaiOpConstant,
                0,
                1,
                DaiOpDefineGlobal,
                0,
                1,
            },
            {
                INTEGER_VAL(1),
                INTEGER_VAL(2),
            },
        },
        {
            "var one = 1;\n one;",
            10,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0,
                DaiOpGetGlobal,
                0,
                0,
                DaiOpPop,
            },
            {
                INTEGER_VAL(1),
            },
        },
        {
            "var one = 1;\n var two = one;\n two;",
            16,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0,
                DaiOpGetGlobal,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                1,
                DaiOpGetGlobal,
                0,
                1,
                DaiOpPop,
            },
            {
                INTEGER_VAL(1),
            },
        },
        {
            "var one = 1;\n one = 1;\n one;",
            16,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0,
                DaiOpConstant,
                0,
                1,
                DaiOpSetGlobal,
                0,
                0,
                DaiOpGetGlobal,
                0,
                0,
                DaiOpPop,
            },
            {
                INTEGER_VAL(1),
                INTEGER_VAL(1),
            },
        },
    };
    run_compiler_tests(tests, sizeof(tests) / sizeof(tests[0]));
    return MUNIT_OK;
}

static MunitResult
test_string_expressions(__attribute__((unused)) const MunitParameter params[],
                        __attribute__((unused)) void* user_data) {
    DaiObjString s = (DaiObjString){{.type = DaiObjType_string}, .chars = "monkey", .length = 6};
    DaiCompilerTestCase tests[] = {
        {
            "\"monkey\";",
            4,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpPop,
            },
            {
                OBJ_VAL(&s),
            },
        },
        {
            "\"monkey\";",
            4,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpPop,
            },
            {
                OBJ_VAL(&s),
            },
        },
    };
    run_compiler_tests(tests, sizeof(tests) / sizeof(tests[0]));
    return MUNIT_OK;
}

static MunitResult
test_functions(__attribute__((unused)) const MunitParameter params[],
               __attribute__((unused)) void* user_data) {
    DaiVM vm;
    DaiVM_init(&vm);
    DaiObjFunction* func1     = DaiObjFunction_New(&vm, "<test1>");
    uint8_t expected_codes1[] = {
        DaiOpConstant,
        0,
        0,
        DaiOpConstant,
        0,
        1,
        DaiOpAdd,
        DaiOpReturnValue,
        DaiOpReturn,
    };
    for (int i = 0; i < sizeof(expected_codes1) / sizeof(expected_codes1[0]); i++) {
        DaiChunk_write(&func1->chunk, expected_codes1[i], 1);
    }

    DaiObjFunction* func2 = DaiObjFunction_New(&vm, "<test2>");
    {
        uint8_t expected_codes2[] = {
            DaiOpConstant,
            0,
            0,
            DaiOpConstant,
            0,
            1,
            DaiOpAdd,
            DaiOpPop,
            DaiOpReturn,
        };
        for (int i = 0; i < sizeof(expected_codes2) / sizeof(expected_codes2[0]); i++) {
            DaiChunk_write(&func2->chunk, expected_codes2[i], 1);
        }
    }

    DaiObjFunction* func3 = DaiObjFunction_New(&vm, "<test2>");
    {
        uint8_t expected_codes2[] = {
            DaiOpReturn,
        };
        for (int i = 0; i < sizeof(expected_codes2) / sizeof(expected_codes2[0]); i++) {
            DaiChunk_write(&func3->chunk, expected_codes2[i], 1);
        }
    }
    DaiObjFunction* func4 = DaiObjFunction_New(&vm, "foo");
    {
        uint8_t expected_codes2[] = {
            DaiOpReturn,
        };
        for (int i = 0; i < sizeof(expected_codes2) / sizeof(expected_codes2[0]); i++) {
            DaiChunk_write(&func4->chunk, expected_codes2[i], 1);
        }
    }

    DaiCompilerTestCase tests[] = {
        {
            "fn() {return 5 + 10;};",
            5,
            {
                DaiOpClosure,
                0,
                0,
                0,
                DaiOpPop,
            },
            {
                INTEGER_VAL(5),
                INTEGER_VAL(10),
                OBJ_VAL(func1),
            },
        },
        {
            "fn() {5 + 10;};",
            5,
            {
                DaiOpClosure,
                0,
                0,
                0,
                DaiOpPop,
            },
            {
                INTEGER_VAL(5),
                INTEGER_VAL(10),
                OBJ_VAL(func2),
            },
        },
        {
            "fn() {};",
            5,
            {
                DaiOpClosure,
                0,
                0,
                0,
                DaiOpPop,
            },
            {
                OBJ_VAL(func3),
            },
        },
        {
            "fn foo() {};",
            7,
            {
                DaiOpClosure,
                0,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0,
            },
            {
                OBJ_VAL(func4),
            },
        },
    };
    run_compiler_tests(tests, sizeof(tests) / sizeof(tests[0]));
    DaiVM_reset(&vm);
    return MUNIT_OK;
}

static MunitResult
test_function_calls(__attribute__((unused)) const MunitParameter params[],
                    __attribute__((unused)) void* user_data) {
    DaiVM vm;
    DaiVM_init(&vm);
    DaiObjFunction* func1 = DaiObjFunction_New(&vm, "<test1>");
    {
        uint8_t expected_codes1[] = {
            DaiOpConstant,
            0,
            0,
            DaiOpPop,
            DaiOpReturn,
        };
        for (int i = 0; i < sizeof(expected_codes1) / sizeof(expected_codes1[0]); i++) {
            DaiChunk_write(&func1->chunk, expected_codes1[i], 1);
        }
    }
    DaiObjFunction* func2 = DaiObjFunction_New(&vm, "<test1>");
    {
        uint8_t expected_codes1[] = {
            DaiOpReturn,
        };
        for (int i = 0; i < sizeof(expected_codes1) / sizeof(expected_codes1[0]); i++) {
            DaiChunk_write(&func2->chunk, expected_codes1[i], 1);
        }
    }
    DaiObjFunction* func3 = DaiObjFunction_New(&vm, "<test1>");
    {
        uint8_t expected_codes1[] = {
            DaiOpReturn,
        };
        for (int i = 0; i < sizeof(expected_codes1) / sizeof(expected_codes1[0]); i++) {
            DaiChunk_write(&func3->chunk, expected_codes1[i], 1);
        }
    }
    DaiObjFunction* func4 = DaiObjFunction_New(&vm, "<test1>");
    {
        uint8_t expected_codes1[] = {
            DaiOpGetLocal,
            1,
            DaiOpReturnValue,
            DaiOpReturn,
        };
        for (int i = 0; i < sizeof(expected_codes1) / sizeof(expected_codes1[0]); i++) {
            DaiChunk_write(&func4->chunk, expected_codes1[i], 1);
        }
    }
    DaiObjFunction* func5 = DaiObjFunction_New(&vm, "<test1>");
    {
        uint8_t expected_codes1[] = {
            DaiOpGetLocal,
            1,
            DaiOpPop,
            DaiOpGetLocal,
            2,
            DaiOpPop,
            DaiOpGetLocal,
            3,
            DaiOpPop,
            DaiOpReturn,
        };
        for (int i = 0; i < sizeof(expected_codes1) / sizeof(expected_codes1[0]); i++) {
            DaiChunk_write(&func5->chunk, expected_codes1[i], 1);
        }
    }
    DaiCompilerTestCase tests[] = {
        {
            "fn() {24;}();",
            7,
            {
                DaiOpClosure,
                0,
                0,
                0,
                DaiOpCall,
                0,
                DaiOpPop,
            },
            {
                INTEGER_VAL(24),
                OBJ_VAL(func1),
            },
        },
        {
            "var noArg = fn() {24;};\n noArg();",
            13,
            {
                DaiOpClosure,
                0,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0,
                DaiOpGetGlobal,
                0,
                0,
                DaiOpCall,
                0,
                DaiOpPop,
            },
            {
                INTEGER_VAL(24),
                OBJ_VAL(func1),
            },
        },
        {
            "var oneArg = fn(a) {};\n oneArg(24);",
            16,
            {
                DaiOpClosure,
                0,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0,
                DaiOpGetGlobal,
                0,
                0,
                DaiOpConstant,
                0,
                1,
                DaiOpCall,
                1,
                DaiOpPop,
            },
            {
                OBJ_VAL(func2),
                INTEGER_VAL(24),
            },
        },
        {
            "var manyArg = fn(a, b, c) {};\n manyArg(24, 25, 26);",
            22,
            {
                DaiOpClosure,
                0,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0,
                DaiOpGetGlobal,
                0,
                0,
                DaiOpConstant,
                0,
                1,
                DaiOpConstant,
                0,
                2,
                DaiOpConstant,
                0,
                3,
                DaiOpCall,
                3,
                DaiOpPop,
            },
            {
                OBJ_VAL(func3),
                INTEGER_VAL(24),
                INTEGER_VAL(25),
                INTEGER_VAL(26),
            },
        },
        {
            "var oneArg = fn(a) { return a; };\n oneArg(24);",
            16,
            {
                DaiOpClosure,
                0,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0,
                DaiOpGetGlobal,
                0,
                0,
                DaiOpConstant,
                0,
                1,
                DaiOpCall,
                1,
                DaiOpPop,
            },
            {
                OBJ_VAL(func4),
                INTEGER_VAL(24),
            },
        },
        {
            "var manyArg = fn(a, b, c) { a; b; c;};\n manyArg(24, 25, 26);",
            22,
            {
                DaiOpClosure,
                0,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0,
                DaiOpGetGlobal,
                0,
                0,
                DaiOpConstant,
                0,
                1,
                DaiOpConstant,
                0,
                2,
                DaiOpConstant,
                0,
                3,
                DaiOpCall,
                3,
                DaiOpPop,
            },
            {
                OBJ_VAL(func5),
                INTEGER_VAL(24),
                INTEGER_VAL(25),
                INTEGER_VAL(26),
            },
        },
    };
    run_compiler_tests(tests, sizeof(tests) / sizeof(tests[0]));
    DaiVM_reset(&vm);
    return MUNIT_OK;
}

static MunitResult
test_var_statement_scopes(__attribute__((unused)) const MunitParameter params[],
                          __attribute__((unused)) void* user_data) {
    DaiVM vm;
    DaiVM_init(&vm);
    DaiObjFunction* func1 = DaiObjFunction_New(&vm, "<test1>");
    {
        uint8_t expected_codes1[] = {
            DaiOpGetGlobal,
            0,
            0,
            DaiOpPop,
            DaiOpReturn,
        };
        for (int i = 0; i < sizeof(expected_codes1) / sizeof(expected_codes1[0]); i++) {
            DaiChunk_write(&func1->chunk, expected_codes1[i], 1);
        }
    }
    DaiObjFunction* func2 = DaiObjFunction_New(&vm, "<test2>");
    {
        uint8_t expected_codes1[] = {
            DaiOpConstant,
            0,
            0,
            DaiOpSetLocal,
            1,
            DaiOpGetLocal,
            1,
            DaiOpPop,
            DaiOpReturn,
        };
        for (int i = 0; i < sizeof(expected_codes1) / sizeof(expected_codes1[0]); i++) {
            DaiChunk_write(&func2->chunk, expected_codes1[i], 1);
        }
    }
    DaiObjFunction* func3 = DaiObjFunction_New(&vm, "<test3>");
    {
        uint8_t expected_codes1[] = {
            DaiOpConstant,
            0,
            0,
            DaiOpSetLocal,
            1,
            DaiOpConstant,
            0,
            1,
            DaiOpSetLocal,
            2,
            DaiOpGetLocal,
            1,
            DaiOpGetLocal,
            2,
            DaiOpAdd,
            DaiOpPop,
            DaiOpReturn,
        };
        for (int i = 0; i < sizeof(expected_codes1) / sizeof(expected_codes1[0]); i++) {
            DaiChunk_write(&func3->chunk, expected_codes1[i], 1);
        }
    }
    DaiCompilerTestCase tests[] = {
        {
            "var num = 55; fn() {num;};",
            11,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0,
                DaiOpClosure,
                0,
                1,
                0,
                DaiOpPop,
            },
            {
                INTEGER_VAL(55),
                OBJ_VAL(func1),
            },
        },
        {
            "fn() {var num = 55; num;};",
            5,
            {
                DaiOpClosure,
                0,
                0,
                0,
                DaiOpPop,
            },
            {
                INTEGER_VAL(55),
                OBJ_VAL(func2),
            },
        },
        {
            "fn() { var a = 55; var b = 77; a + b; };",
            5,
            {
                DaiOpClosure,
                0,
                0,
                0,
                DaiOpPop,
            },
            {
                INTEGER_VAL(55),
                INTEGER_VAL(77),
                OBJ_VAL(func3),
            },
        },
    };
    run_compiler_tests(tests, sizeof(tests) / sizeof(tests[0]));
    DaiVM_reset(&vm);
    return MUNIT_OK;
}

static MunitResult
test_builtins(__attribute__((unused)) const MunitParameter params[],
              __attribute__((unused)) void* user_data) {
    DaiVM vm;
    DaiVM_init(&vm);
    DaiObjString s = (DaiObjString){{.type = DaiObjType_string}, .chars = "monkey", .length = 6};
    DaiObjFunction* func = DaiObjFunction_New(&vm, "<test2>");
    {
        uint8_t expected_codes2[] = {
            DaiOpGetBuiltin,
            1,
            DaiOpConstant,
            0,
            0,
            DaiOpCall,
            1,
            DaiOpPop,
            DaiOpReturn,
        };
        for (int i = 0; i < sizeof(expected_codes2) / sizeof(expected_codes2[0]); i++) {
            DaiChunk_write(&func->chunk, expected_codes2[i], 1);
        }
    }

    DaiCompilerTestCase tests[] = {
        {
            "len(\"monkey\");",
            8,
            {
                DaiOpGetBuiltin,
                1,
                DaiOpConstant,
                0,
                0,
                DaiOpCall,
                1,
                DaiOpPop,
            },
            {
                OBJ_VAL(&s),
            },
        },
        {
            "fn() {len(\"monkey\");};",
            5,
            {
                DaiOpClosure,
                0,
                0,
                0,
                DaiOpPop,
            },
            {
                OBJ_VAL(&s),
                OBJ_VAL(func),
            },
        },
    };
    run_compiler_tests(tests, sizeof(tests) / sizeof(tests[0]));
    DaiVM_reset(&vm);
    return MUNIT_OK;
}

static MunitResult
test_closures(__attribute__((unused)) const MunitParameter params[],
              __attribute__((unused)) void* user_data) {
    DaiVM vm;
    DaiVM_init(&vm);
    DaiObjFunction* func1_1 = DaiObjFunction_New(&vm, "<test1_1>");
    {
        uint8_t expected_codes2[] = {
            DaiOpGetFree,
            0,
            DaiOpGetLocal,
            1,
            DaiOpAdd,
            DaiOpPop,
            DaiOpReturn,
        };
        for (int i = 0; i < sizeof(expected_codes2) / sizeof(expected_codes2[0]); i++) {
            DaiChunk_write(&func1_1->chunk, expected_codes2[i], 1);
        }
    }
    DaiObjFunction* func1_2 = DaiObjFunction_New(&vm, "<test1_2>");
    {
        uint8_t expected_codes2[] = {
            DaiOpGetLocal,
            1,
            DaiOpClosure,
            0,
            0,
            1,
            DaiOpPop,
            DaiOpReturn,
        };
        for (int i = 0; i < sizeof(expected_codes2) / sizeof(expected_codes2[0]); i++) {
            DaiChunk_write(&func1_2->chunk, expected_codes2[i], 1);
        }
    }
    DaiObjFunction* func2_1 = DaiObjFunction_New(&vm, "<test2>");
    {
        uint8_t expected_codes2[] = {
            DaiOpGetFree,
            0,
            DaiOpGetFree,
            1,
            DaiOpAdd,
            DaiOpGetLocal,
            1,
            DaiOpAdd,
            DaiOpPop,
            DaiOpReturn,
        };
        for (int i = 0; i < sizeof(expected_codes2) / sizeof(expected_codes2[0]); i++) {
            DaiChunk_write(&func2_1->chunk, expected_codes2[i], 1);
        }
    }
    DaiObjFunction* func2_2 = DaiObjFunction_New(&vm, "<test2>");
    {
        uint8_t expected_codes2[] = {
            DaiOpGetFree,
            0,
            DaiOpGetLocal,
            1,
            DaiOpClosure,
            0,
            0,
            2,
            DaiOpPop,
            DaiOpReturn,
        };
        for (int i = 0; i < sizeof(expected_codes2) / sizeof(expected_codes2[0]); i++) {
            DaiChunk_write(&func2_2->chunk, expected_codes2[i], 1);
        }
    }
    DaiObjFunction* func2_3 = DaiObjFunction_New(&vm, "<test2>");
    {
        uint8_t expected_codes2[] = {
            DaiOpGetLocal,
            1,
            DaiOpClosure,
            0,
            0,
            1,
            DaiOpPop,
            DaiOpReturn,
        };
        for (int i = 0; i < sizeof(expected_codes2) / sizeof(expected_codes2[0]); i++) {
            DaiChunk_write(&func2_3->chunk, expected_codes2[i], 1);
        }
    }
    DaiObjFunction* func3_1 = DaiObjFunction_New(&vm, "<test3_1>");
    {
        uint8_t expected_codes2[] = {
            DaiOpConstant,
            0,
            0,
            DaiOpSetLocal,
            1,
            DaiOpGetGlobal,
            0,
            0,
            DaiOpGetFree,
            0,
            DaiOpAdd,
            DaiOpGetFree,
            1,
            DaiOpAdd,
            DaiOpGetLocal,
            1,
            DaiOpAdd,
            DaiOpReturnValue,
            DaiOpReturn,
        };
        for (int i = 0; i < sizeof(expected_codes2) / sizeof(expected_codes2[0]); i++) {
            DaiChunk_write(&func3_1->chunk, expected_codes2[i], 1);
        }
    }
    DaiObjFunction* func3_2 = DaiObjFunction_New(&vm, "<test3_2>");
    {
        uint8_t expected_codes2[] = {
            DaiOpConstant,
            0,
            0,
            DaiOpSetLocal,
            1,
            DaiOpGetFree,
            0,
            DaiOpGetLocal,
            1,
            DaiOpClosure,
            0,
            1,
            2,
            DaiOpPop,
            DaiOpReturn,
        };
        for (int i = 0; i < sizeof(expected_codes2) / sizeof(expected_codes2[0]); i++) {
            DaiChunk_write(&func3_2->chunk, expected_codes2[i], 1);
        }
    }
    DaiObjFunction* func3_3 = DaiObjFunction_New(&vm, "<test3_3>");
    {
        uint8_t expected_codes2[] = {
            DaiOpConstant,
            0,
            0,
            DaiOpSetLocal,
            1,
            DaiOpGetLocal,
            1,
            DaiOpClosure,
            0,
            1,
            1,
            DaiOpPop,
            DaiOpReturn,
        };
        for (int i = 0; i < sizeof(expected_codes2) / sizeof(expected_codes2[0]); i++) {
            DaiChunk_write(&func3_3->chunk, expected_codes2[i], 1);
        }
    }
    DaiCompilerTestCase tests[] = {
        {
            "fn(a) {fn(b) {a + b;};};",
            5,
            {
                DaiOpClosure,
                0,
                0,
                0,
                DaiOpPop,
            },
            {
                OBJ_VAL(func1_1),
                OBJ_VAL(func1_2),
            },
        },
        {
            "fn(a) {\n"
            "    fn(b) {\n"
            "        fn(c) {\n"
            "            a + b + c;\n"
            "        };\n"
            "    };\n"
            "};",
            5,
            {
                DaiOpClosure,
                0,
                0,
                0,
                DaiOpPop,
            },
            {
                OBJ_VAL(func2_1),
                OBJ_VAL(func2_2),
                OBJ_VAL(func2_3),
            },
        },
        {
            "var global = 55;\n"
            "fn() {\n"
            "    var a = 66;\n"
            "    fn() {\n"
            "        var b = 77;\n"
            "        fn() {\n"
            "            var c = 88;\n"
            "            return global + a + b + c;\n"
            "        };\n"
            "    };\n"
            "};",
            11,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0,
                DaiOpClosure,
                0,
                1,
                0,
                DaiOpPop,
            },
            {
                INTEGER_VAL(55),
                INTEGER_VAL(66),
                INTEGER_VAL(77),
                INTEGER_VAL(88),
                OBJ_VAL(func3_1),
                OBJ_VAL(func3_2),
                OBJ_VAL(func3_3),
            },
        },

    };
    run_compiler_tests(tests, sizeof(tests) / sizeof(tests[0]));
    DaiVM_reset(&vm);
    return MUNIT_OK;
}

static MunitResult
test_class(__attribute__((unused)) const MunitParameter params[],
           __attribute__((unused)) void* user_data) {
    DaiVM vm;
    DaiVM_init(&vm);
    DaiObjString sa = (DaiObjString){{.type = DaiObjType_string}, .chars = "B", .length = 1};
    DaiObjString s  = (DaiObjString){{.type = DaiObjType_string}, .chars = "C", .length = 1};
    DaiObjString s2 = (DaiObjString){{.type = DaiObjType_string}, .chars = "s2", .length = 2};
    DaiObjString s3 = (DaiObjString){{.type = DaiObjType_string}, .chars = "s3", .length = 2};
    DaiObjString s4 = (DaiObjString){{.type = DaiObjType_string}, .chars = "s4", .length = 2};
    DaiObjString s5 = (DaiObjString){{.type = DaiObjType_string}, .chars = "classVar", .length = 8};
    DaiObjString s6 = (DaiObjString){{.type = DaiObjType_string}, .chars = "func2", .length = 5};
    DaiObjFunction* func = DaiObjFunction_New(&vm, "s4");
    {
        uint8_t expected_codes2[] = {
            DaiOpReturn,
        };
        for (int i = 0; i < sizeof(expected_codes2) / sizeof(expected_codes2[0]); i++) {
            DaiChunk_write(&func->chunk, expected_codes2[i], 1);
        }
    }
    DaiObjFunction* func2 = DaiObjFunction_New(&vm, "func2");
    {
        /* 常量
            classVar
            5
            classVar
            func2
            6
            s2
            s3
        */
        uint8_t expected_codes2[] = {
            // self;
            DaiOpGetLocal,
            0,
            DaiOpPop,
            // self.classVar;
            DaiOpGetSelfProperty,
            0,
            0,
            DaiOpPop,
            // self.classVar = 5;
            DaiOpConstant,
            0,
            1,
            DaiOpSetSelfProperty,
            0,
            2,
            // super.func2;
            DaiOpGetSuperProperty,
            0,
            3,
            DaiOpPop,
            // var c = C();
            DaiOpGetGlobal,
            0,
            0,
            DaiOpCall,
            0,
            DaiOpSetLocal,
            1,
            // c.s2 = 6;
            DaiOpConstant,
            0,
            4,
            DaiOpGetLocal,
            1,
            DaiOpSetProperty,
            0,
            5,
            // c.s3;
            DaiOpGetLocal,
            1,
            DaiOpGetProperty,
            0,
            6,
            DaiOpPop,
            DaiOpReturn,
        };
        for (int i = 0; i < sizeof(expected_codes2) / sizeof(expected_codes2[0]); i++) {
            DaiChunk_write(&func2->chunk, expected_codes2[i], 1);
        }
    }
    DaiObjString s6_2 =
        (DaiObjString){{.type = DaiObjType_string}, .chars = "func2_2", .length = 7};
    DaiObjFunction* func2_2 = DaiObjFunction_New(&vm, "func2_2");
    {
        /* 常量
            classVar
            5
            classVar
            func2
            6
            s2
            s3
        */
        uint8_t expected_codes2[] = {
            // self;
            DaiOpGetLocal,
            0,
            DaiOpPop,
            // self.classVar;
            DaiOpGetSelfProperty,
            0,
            0,
            DaiOpPop,
            // self.classVar = 5;
            DaiOpConstant,
            0,
            1,
            DaiOpSetSelfProperty,
            0,
            2,
            // super.func2;
            DaiOpGetSuperProperty,
            0,
            3,
            DaiOpPop,
            // var c = C();
            DaiOpGetGlobal,
            0,
            1,
            DaiOpCall,
            0,
            DaiOpSetLocal,
            1,
            // c.s2 = 6;
            DaiOpConstant,
            0,
            4,
            DaiOpGetLocal,
            1,
            DaiOpSetProperty,
            0,
            5,
            // c.s3;
            DaiOpGetLocal,
            1,
            DaiOpGetProperty,
            0,
            6,
            DaiOpPop,
            DaiOpReturn,
        };
        for (int i = 0; i < sizeof(expected_codes2) / sizeof(expected_codes2[0]); i++) {
            DaiChunk_write(&func2_2->chunk, expected_codes2[i], 1);
        }
    }
    DaiCompilerTestCase tests[] = {
        {
            "class C {};\n",
            10,
            {
                DaiOpClass,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0,
                DaiOpGetGlobal,
                0,
                0,
                DaiOpPop,
            },
            {
                OBJ_VAL(&s),
            },
        },
        {
            "class C {\n"
            "  var s2;\n"
            "};\n",
            14,
            {
                DaiOpClass,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0,
                DaiOpGetGlobal,
                0,
                0,
                DaiOpUndefined,
                DaiOpDefineField,
                0,
                1,
                DaiOpPop,
            },
            {
                OBJ_VAL(&s),
                OBJ_VAL(&s2),
            },
        },
        {
            "class C {\n"
            "  var s2;\n"
            "  var s3 = 0;\n"
            "};\n",
            20,
            {
                DaiOpClass,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0,
                DaiOpGetGlobal,
                0,
                0,

                DaiOpUndefined,
                DaiOpDefineField,
                0,
                1,

                DaiOpConstant,
                0,
                2,
                DaiOpDefineField,
                0,
                3,

                DaiOpPop,
            },
            {
                OBJ_VAL(&s),
                OBJ_VAL(&s2),
                INTEGER_VAL(0),
                OBJ_VAL(&s3),
            },
        },
        {
            "class C {\n"
            "  var s2;\n"
            "  var s3 = 0;\n"
            "  fn s4() {};\n"
            "};\n",
            27,
            {
                DaiOpClass,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0,
                DaiOpGetGlobal,
                0,
                0,

                DaiOpUndefined,
                DaiOpDefineField,
                0,
                1,

                DaiOpConstant,
                0,
                2,
                DaiOpDefineField,
                0,
                3,

                DaiOpClosure,
                0,
                4,
                0,
                DaiOpDefineMethod,
                0,
                5,

                DaiOpPop,
            },
            {
                OBJ_VAL(&s),
                OBJ_VAL(&s2),
                INTEGER_VAL(0),
                OBJ_VAL(&s3),
                OBJ_VAL(func),
                OBJ_VAL(&s4),
            },
        },
        {
            "class C {\n"
            "  var s2;\n"
            "  var s3 = 0;\n"
            "  fn s4() {};\n"
            "  class var classVar = 3;\n"
            "};\n",
            33,
            {
                DaiOpClass,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0,
                DaiOpGetGlobal,
                0,
                0,

                DaiOpUndefined,
                DaiOpDefineField,
                0,
                1,

                DaiOpConstant,
                0,
                2,
                DaiOpDefineField,
                0,
                3,

                DaiOpClosure,
                0,
                4,
                0,
                DaiOpDefineMethod,
                0,
                5,

                DaiOpConstant,
                0,
                6,
                DaiOpDefineClassField,
                0,
                7,
                DaiOpPop,
            },
            {
                OBJ_VAL(&s),
                OBJ_VAL(&s2),
                INTEGER_VAL(0),
                OBJ_VAL(&s3),
                OBJ_VAL(func),
                OBJ_VAL(&s4),
                INTEGER_VAL(3),
                OBJ_VAL(&s5),
            },
        },
        {
            "class C {\n"
            "  var s2;\n"
            "  var s3 = 0;\n"
            "  fn s4() {};\n"
            "  class var classVar = 3;\n"
            "  class fn func2() {\n"
            "    self;\n"
            "    self.classVar;\n"
            "    self.classVar = 5;\n"
            "    super.func2;\n"
            "    var c = C();\n"
            "    c.s2 = 6;\n"
            "    c.s3;\n"
            "  };\n"
            "};\n",
            40,
            {
                DaiOpClass,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0,
                DaiOpGetGlobal,
                0,
                0,

                DaiOpUndefined,
                DaiOpDefineField,
                0,
                1,

                DaiOpConstant,
                0,
                2,
                DaiOpDefineField,
                0,
                3,

                DaiOpClosure,
                0,
                4,
                0,
                DaiOpDefineMethod,
                0,
                5,

                DaiOpConstant,
                0,
                6,
                DaiOpDefineClassField,
                0,
                7,

                DaiOpClosure,
                0,
                8,
                0,
                DaiOpDefineClassMethod,
                0,
                9,
                DaiOpPop,
            },
            {
                OBJ_VAL(&s),
                OBJ_VAL(&s2),
                INTEGER_VAL(0),
                OBJ_VAL(&s3),
                OBJ_VAL(func),
                OBJ_VAL(&s4),
                INTEGER_VAL(3),
                OBJ_VAL(&s5),

                // 方法 func2 内的常量
                OBJ_VAL(&s5),
                INTEGER_VAL(5),
                OBJ_VAL(&s5),
                OBJ_VAL(&s6),
                INTEGER_VAL(6),
                OBJ_VAL(&s2),
                OBJ_VAL(&s3),

                OBJ_VAL(func2),
                OBJ_VAL(&s6),
            },
        },
        {
            "class B {};\n"
            "class C < B {\n"
            "  var s2;\n"
            "  var s3 = 0;\n"
            "  fn s4() {};\n"
            "  class var classVar = 3;\n"
            "  class fn func2_2() {\n"
            "    self;\n"
            "    self.classVar;\n"
            "    self.classVar = 5;\n"
            "    super.func2_2;\n"
            "    var c = C();\n"
            "    c.s2 = 6;\n"
            "    c.s3;\n"
            "  };\n"
            "};\n",
            54,
            {
                DaiOpClass,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0,
                DaiOpGetGlobal,
                0,
                0,
                DaiOpPop,

                DaiOpClass,
                0,
                1,
                DaiOpGetGlobal,
                0,
                0,
                DaiOpInherit,
                DaiOpDefineGlobal,
                0,
                1,
                DaiOpGetGlobal,
                0,
                1,

                DaiOpUndefined,
                DaiOpDefineField,
                0,
                2,

                DaiOpConstant,
                0,
                3,
                DaiOpDefineField,
                0,
                4,

                DaiOpClosure,
                0,
                5,
                0,
                DaiOpDefineMethod,
                0,
                6,

                DaiOpConstant,
                0,
                7,
                DaiOpDefineClassField,
                0,
                8,

                DaiOpClosure,
                0,
                9,
                0,
                DaiOpDefineClassMethod,
                0,
                10,
                DaiOpPop,
            },
            {
                OBJ_VAL(&sa),
                OBJ_VAL(&s),
                OBJ_VAL(&s2),
                INTEGER_VAL(0),
                OBJ_VAL(&s3),
                OBJ_VAL(func),
                OBJ_VAL(&s4),
                INTEGER_VAL(3),
                OBJ_VAL(&s5),

                // 方法 func2_2 内的常量
                OBJ_VAL(&s5),
                INTEGER_VAL(5),
                OBJ_VAL(&s5),
                OBJ_VAL(&s6_2),
                INTEGER_VAL(6),
                OBJ_VAL(&s2),
                OBJ_VAL(&s3),

                OBJ_VAL(func2_2),
                OBJ_VAL(&s6_2),
            },
        },
    };
    run_compiler_tests(tests, sizeof(tests) / sizeof(tests[0]));
    DaiVM_reset(&vm);
    return MUNIT_OK;
}

static MunitResult
test_compile_error(__attribute__((unused)) const MunitParameter params[],
                   __attribute__((unused)) void* user_data) {
    struct {
        const char* input;
        const char* expected_error_msg;
    } tests[] = {
        {
            "var a = 1;\nvar a = 2;",
            "CompileError: symbol 'a' already defined in <test>:2:5",
        },
        {
            "a;",
            "CompileError: undefined variable: 'a' in <test>:1:1",
        },
        {
            "a = 1;",
            "CompileError: undefined variable: 'a' in <test>:1:1",
        },
        {
            "a; var a = 1;",
            "CompileError: undefined variable: 'a' in <test>:1:1",
        },
        {
            "class Foo {var a; fn a() {};};",
            "CompileError: property 'a' already defined in <test>:1:19",
        },
    };
    for (int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        DaiVM vm;
        DaiVM_init(&vm);
        DaiObjFunction* func = DaiObjFunction_New(&vm, "<test>");
        DaiCompileError* err = compile_error_helper(tests[i].input, func, &vm);
        munit_assert_not_null(err);
        char* msg = DaiCompileError_string(err);
        munit_assert_string_equal(msg, tests[i].expected_error_msg);
        free(msg);
        DaiCompileError_free(err);
        DaiVM_reset(&vm);
    }
    return MUNIT_OK;
}

MunitTest compile_tests[] = {
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
    {(char*)"/test_functions", test_functions, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/test_function_calls", test_function_calls, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/test_var_statement_scopes",
     test_var_statement_scopes,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/test_builtins", test_builtins, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/test_closures", test_closures, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/test_class", test_class, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/test_compile_error", test_compile_error, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};
