#include <stdio.h>

#include "dai_chunk.h"
#include "dai_value.h"
#include "munit/munit.h"

#include "dai_compile.h"
#include "dai_debug.h"
#include "dai_object.h"
#include "dai_parse.h"
#include "dai_tokenize.h"

void
dai_assert_value_equal(DaiValue actual, DaiValue expected);

static void
compile_helper(const char* input, DaiObjModule* module, DaiVM* vm) {
    // 词法分析
    DaiTokenList* tlist = DaiTokenList_New();
    DaiError* err       = dai_tokenize_string(input, tlist);
    if (err) {
        DaiSyntaxError_setFilename(err, "<test>");
        DaiSyntaxError_pprint(err, input);
    }
    munit_assert_null(err);
    DaiAstProgram program;
    DaiAstProgram_init(&program);
    err = dai_parse(tlist, &program);
    if (err) {
        DaiSyntaxError_setFilename(err, "<test>");
        DaiSyntaxError_pprint(err, input);
    }
    munit_assert_null(err);
    {
        // char *s = program->string_fn((DaiAstBase*)program, true);
        // printf(s);
        // free(s);
    }
    DaiTokenList_free(tlist);
    err = dai_compile(&program, module, vm);
    if (err) {
        DaiCompileError_pprint(err, input);
    }
    DaiAstProgram_reset(&program);
    munit_assert_null(err);
}

static DaiCompileError*
compile_error_helper(const char* input, DaiObjModule* module, DaiVM* vm) {
    // 词法分析
    DaiTokenList* tlist = DaiTokenList_New();
    DaiError* err       = dai_tokenize_string(input, tlist);
    if (err) {
        DaiSyntaxError_setFilename(err, "<test>");
        DaiSyntaxError_pprint(err, input);
    }
    munit_assert_null(err);
    DaiAstProgram program;
    DaiAstProgram_init(&program);
    err = dai_parse(tlist, &program);
    if (err) {
        DaiSyntaxError_setFilename(err, "<test>");
        DaiSyntaxError_pprint(err, input);
    }
    munit_assert_null(err);
    DaiTokenList_free(tlist);
    err = dai_compile(&program, module, vm);
    DaiAstProgram_reset(&program);
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

static DaiObjModule*
create_test_module(DaiVM* vm) {
    const char* name     = strdup("<test>");
    const char* filename = strdup("<test-file>");
    DaiObjModule* module = DaiObjModule_New(vm, name, filename);
    return module;
}

static void
run_compiler_tests(const DaiCompilerTestCase* tests, const size_t count) {
    for (size_t i = 0; i < count; i++) {
#ifdef DAI_TEST_VERBOSE
        printf("=========================== %zu \n", i);
        printf("%s\n", tests[i].input);
#endif
        DaiVM vm;
        DaiVM_init(&vm);
        DaiObjModule* module = create_test_module(&vm);
        compile_helper(tests[i].input, module, &vm);
        DaiChunk chunk = module->chunk;
        if (chunk.count != tests[i].expected_count) {
            DaiChunk_disassemble(&chunk, "test actual");
        }
        munit_assert_int(chunk.count, ==, tests[i].expected_count);
        for (int j = 0; j < chunk.count; j++) {
            if (chunk.code[j] != tests[i].expected_codes[j]) {
                DaiChunk_disassemble(&chunk, "test");
            }
            if (chunk.code[j] != tests[i].expected_codes[j]) {
                printf("unexpected at %d\n", j);
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
    const DaiCompilerTestCase tests[] = {
        {
            "1 + 2;",
            8 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpConstant,
                0,
                1,
                DaiOpAdd,
                DaiOpPop,
                DaiOpEnd,
            },
            {
                INTEGER_VAL(1),
                INTEGER_VAL(2),
            },
        },
        {
            "1; 2;",
            8 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpPop,
                DaiOpConstant,
                0,
                1,
                DaiOpPop,
                DaiOpEnd,
            },
            {
                INTEGER_VAL(1),
                INTEGER_VAL(2),
            },

        },
        {
            "1 - 2;",
            8 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpConstant,
                0,
                1,
                DaiOpSub,
                DaiOpPop,
                DaiOpEnd,
            },
            {
                INTEGER_VAL(1),
                INTEGER_VAL(2),
            },
        },
        {
            "1 * 2;",
            8 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpConstant,
                0,
                1,
                DaiOpMul,
                DaiOpPop,
                DaiOpEnd,
            },
            {
                INTEGER_VAL(1),
                INTEGER_VAL(2),
            },
        },
        {
            "2 / 1;",
            8 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpConstant,
                0,
                1,
                DaiOpDiv,
                DaiOpPop,
                DaiOpEnd,
            },
            {
                INTEGER_VAL(2),
                INTEGER_VAL(1),
            },
        },
        {
            "2 % 1;",
            8 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpConstant,
                0,
                1,
                DaiOpMod,
                DaiOpPop,
                DaiOpEnd,
            },
            {
                INTEGER_VAL(2),
                INTEGER_VAL(1),
            },
        },

        {
            "-1;",
            5 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpMinus,
                DaiOpPop,
                DaiOpEnd,
            },
            {
                INTEGER_VAL(1),
            },
        },
        {
            "~1;",
            5 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpBitwiseNot,
                DaiOpPop,
                DaiOpEnd,
            },
            {
                INTEGER_VAL(1),
            },
        },
        {
            "-1.0;",
            5 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpMinus,
                DaiOpPop,
                DaiOpEnd,
            },
            {
                FLOAT_VAL(1.0),
            },
        },
        {
            "3.1415926;",
            4 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpPop,
                DaiOpEnd,
            },
            {
                FLOAT_VAL(3.1415926),
            },
        },
        {
            "2 << 1;",
            9 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpConstant,
                0,
                1,
                DaiOpBinary,
                BinaryOpLeftShift,
                DaiOpPop,
                DaiOpEnd,
            },
            {
                INTEGER_VAL(2),
                INTEGER_VAL(1),
            },

        },
        {
            "2 >> 1;",
            9 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpConstant,
                0,
                1,
                DaiOpBinary,
                BinaryOpRightShift,
                DaiOpPop,
                DaiOpEnd,
            },
            {
                INTEGER_VAL(2),
                INTEGER_VAL(1),
            },
        },
        {
            "2 & 1;",
            9 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpConstant,
                0,
                1,
                DaiOpBinary,
                BinaryOpBitwiseAnd,
                DaiOpPop,
                DaiOpEnd,
            },
            {
                INTEGER_VAL(2),
                INTEGER_VAL(1),
            },
        },
        {
            "2 | 1;",
            9 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpConstant,
                0,
                1,
                DaiOpBinary,
                BinaryOpBitwiseOr,
                DaiOpPop,
                DaiOpEnd,
            },
            {
                INTEGER_VAL(2),
                INTEGER_VAL(1),
            },
        },
        {
            "2 ^ 1;",
            9 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpConstant,
                0,
                1,
                DaiOpBinary,
                BinaryOpBitwiseXor,
                DaiOpPop,
                DaiOpEnd,
            },
            {
                INTEGER_VAL(2),
                INTEGER_VAL(1),
            },

        },
    };
    run_compiler_tests(tests, sizeof(tests) / sizeof(tests[0]));
    return MUNIT_OK;
}

static MunitResult
test_boolean_expressions(__attribute__((unused)) const MunitParameter params[],
                         __attribute__((unused)) void* user_data) {
    const DaiCompilerTestCase tests[] = {
        {
            "true;",
            2 + 1,
            {
                DaiOpTrue,
                DaiOpPop,
                DaiOpEnd,
            },
        },
        {
            "false;",
            2 + 1,
            {
                DaiOpFalse,
                DaiOpPop,
                DaiOpEnd,
            },
        },
        {
            "nil;",
            2 + 1,
            {
                DaiOpNil,
                DaiOpPop,
                DaiOpEnd,
            },
        },

        {
            "1 > 2;",
            8 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpConstant,
                0,
                1,
                DaiOpGreaterThan,
                DaiOpPop,
                DaiOpEnd,
            },
            {
                INTEGER_VAL(1),
                INTEGER_VAL(2),
            },
        },
        {
            "1 < 2;",
            8 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpConstant,
                0,
                1,
                DaiOpGreaterThan,
                DaiOpPop,
                DaiOpEnd,
            },
            {
                INTEGER_VAL(2),
                INTEGER_VAL(1),
            },
        },
        {
            "1 >= 2;",
            8 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpConstant,
                0,
                1,
                DaiOpGreaterEqualThan,
                DaiOpPop,
                DaiOpEnd,
            },
            {
                INTEGER_VAL(1),
                INTEGER_VAL(2),
            },
        },
        {
            "1 <= 2;",
            8 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpConstant,
                0,
                1,
                DaiOpGreaterEqualThan,
                DaiOpPop,
                DaiOpEnd,
            },
            {
                INTEGER_VAL(2),
                INTEGER_VAL(1),
            },
        },
        {
            "1 == 2;",
            8 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpConstant,
                0,
                1,
                DaiOpEqual,
                DaiOpPop,
                DaiOpEnd,
            },
            {
                INTEGER_VAL(1),
                INTEGER_VAL(2),
            },
        },
        {
            "1 != 2;",
            8 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpConstant,
                0,
                1,
                DaiOpNotEqual,
                DaiOpPop,
                DaiOpEnd,
            },
            {
                INTEGER_VAL(1),
                INTEGER_VAL(2),
            },
        },
        {
            "true == false;",
            4 + 1,
            {
                DaiOpTrue,
                DaiOpFalse,
                DaiOpEqual,
                DaiOpPop,
                DaiOpEnd,
            },
        },
        {
            "true != false;",
            4 + 1,
            {
                DaiOpTrue,
                DaiOpFalse,
                DaiOpNotEqual,
                DaiOpPop,
                DaiOpEnd,
            },
        },
        {
            "!true;",
            3 + 1,
            {
                DaiOpTrue,
                DaiOpBang,
                DaiOpPop,
                DaiOpEnd,
            },
        },
        {
            "nil != nil;",
            4 + 1,
            {
                DaiOpNil,
                DaiOpNil,
                DaiOpNotEqual,
                DaiOpPop,
                DaiOpEnd,
            },
        },
        {
            "!nil;",
            3 + 1,
            {
                DaiOpNil,
                DaiOpBang,
                DaiOpPop,
                DaiOpEnd,
            },
        },
        {
            "not nil;",
            3 + 1,
            {
                DaiOpNil,
                DaiOpNot,
                DaiOpPop,
                DaiOpEnd,
            },
        },
        {
            "nil and nil;",
            6 + 1,
            {
                DaiOpNil,
                DaiOpAndJump,
                0,
                1,
                DaiOpNil,
                DaiOpPop,
                DaiOpEnd,
            },
        },
        {
            "nil or nil;",
            6 + 1,
            {
                DaiOpNil,
                DaiOpOrJump,
                0,
                1,
                DaiOpNil,
                DaiOpPop,
                DaiOpEnd,
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
            12 + 1,
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
                DaiOpEnd,
            },
            {
                INTEGER_VAL(10),
                INTEGER_VAL(3333),
            },
        },
        {
            "if (true) {10;} \nelse {20;};\n 3333;",
            19 + 1,
            {
                DaiOpTrue, DaiOpJumpIfFalse, 0, 7, DaiOpConstant, 0,        0,
                DaiOpPop,  DaiOpJump,        0, 4, DaiOpConstant, 0,        1,
                DaiOpPop,  DaiOpConstant,    0, 2, DaiOpPop,      DaiOpEnd,
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
            19 + 1,
            {
                DaiOpTrue, DaiOpJumpIfFalse, 0, 7, DaiOpConstant, 0, 0, DaiOpPop, DaiOpJump, 0, 8,
                DaiOpTrue, DaiOpJumpIfFalse, 0, 4, DaiOpConstant, 0, 1, DaiOpPop, DaiOpEnd,
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
            26 + 1,
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
                DaiOpEnd,
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
            37 + 1,
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
                DaiOpEnd,
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
            7 + 1,
            {
                DaiOpFalse,
                DaiOpJumpIfFalse,
                0,
                3,
                DaiOpJumpBack,
                0,
                7,
                DaiOpEnd,
            },
            {},
        },
        {
            "while (false) { 1; }",
            11 + 1,
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
                DaiOpEnd,
            },
            {
                INTEGER_VAL(1),
            },
        },
        {
            "while (false) { 1; break; }",
            14 + 1,
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
                DaiOpEnd,
            },
            {
                INTEGER_VAL(1),
            },
        },
        {
            "while (false) { 1; continue; }",
            14 + 1,
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
                DaiOpEnd,
            },
            {
                INTEGER_VAL(1),
            },
        },
        {
            "while (false) { break; 1; break; }",
            17 + 1,
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
                DaiOpEnd,
            },
            {
                INTEGER_VAL(1),
            },
        },
        {
            "while (false) { continue; 1; continue; }",
            17 + 1,
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
                DaiOpEnd,
            },
            {
                INTEGER_VAL(1),
            },
        },
        {
            "while (true) { while (false) { break; 1; break; } break; }",
            27 + 1,
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
                DaiOpEnd,
            },
            {
                INTEGER_VAL(1),
            },
        },
        {
            "while (true) {\n"
            "  break;\n"
            "  while (false) {\n"
            "    break;\n"
            "    1;\n"
            "    break;\n"
            "  }\n"
            "  break;\n"
            "}\n"
            "",
            30 + 1,
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
                DaiOpEnd,
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
            33 + 1,
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
                DaiOpEnd,
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
            36 + 1,
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
                DaiOpEnd,
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
            12 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpConstant,
                0,
                1,
                DaiOpDefineGlobal,
                0,
                1 + BUILTIN_GLOBALS_COUNT,
                DaiOpEnd,
            },
            {
                INTEGER_VAL(1),
                INTEGER_VAL(2),
            },
        },
        {
            "var one = 1;\n one;",
            10 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpGetGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpPop,
                DaiOpEnd,
            },
            {
                INTEGER_VAL(1),
            },
        },
        {
            "var one = 1;\n var two = one;\n two;",
            16 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpGetGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpDefineGlobal,
                0,
                1 + BUILTIN_GLOBALS_COUNT,
                DaiOpGetGlobal,
                0,
                1 + BUILTIN_GLOBALS_COUNT,
                DaiOpPop,
                DaiOpEnd,
            },
            {
                INTEGER_VAL(1),
            },
        },
        {
            "var one = 1;\n one = 1;\n one;",
            16 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpConstant,
                0,
                1,
                DaiOpSetGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpGetGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpPop,
                DaiOpEnd,
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
            4 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpPop,
                DaiOpEnd,
            },
            {
                OBJ_VAL(&s),
            },
        },
        {
            "\"monkey\";",
            4 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpPop,
                DaiOpEnd,
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
    DaiObjModule* module      = create_test_module(&vm);
    DaiObjFunction* func1     = DaiObjFunction_New(&vm, module, "<test1>", "<test>");
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

    DaiObjFunction* func2 = DaiObjFunction_New(&vm, module, "<test2>", "<test>");
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

    DaiObjFunction* func3 = DaiObjFunction_New(&vm, module, "<test2>", "<test>");
    {
        uint8_t expected_codes2[] = {
            DaiOpReturn,
        };
        for (int i = 0; i < sizeof(expected_codes2) / sizeof(expected_codes2[0]); i++) {
            DaiChunk_write(&func3->chunk, expected_codes2[i], 1);
        }
    }
    DaiObjFunction* func4 = DaiObjFunction_New(&vm, module, "foo", "<test>");
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
            4 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpPop,
                DaiOpEnd,
            },
            {
                INTEGER_VAL(5),
                INTEGER_VAL(10),
                OBJ_VAL(func1),
            },
        },
        {
            "fn() {5 + 10;};",
            4 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpPop,
                DaiOpEnd,
            },
            {
                INTEGER_VAL(5),
                INTEGER_VAL(10),
                OBJ_VAL(func2),
            },
        },
        {
            "fn() {};",
            4 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpPop,
                DaiOpEnd,
            },
            {
                OBJ_VAL(func3),
            },
        },
        {
            "fn foo() {};",
            6 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpEnd,
            },
            {
                OBJ_VAL(func4),
            },
        },
        {
            "fn foo(a=1) {};",
            11 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpConstant,
                0,
                1,
                DaiOpSetFunctionDefault,
                1,
                DaiOpDefineGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpEnd,
            },
            {
                OBJ_VAL(func4),
                INTEGER_VAL(1),
            },
        },
        {
            "fn foo(a, b=1, c=2) {};",
            14 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpConstant,
                0,
                1,
                DaiOpConstant,
                0,
                2,
                DaiOpSetFunctionDefault,
                2,
                DaiOpDefineGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpEnd,
            },
            {
                OBJ_VAL(func4),
                INTEGER_VAL(1),
                INTEGER_VAL(2),
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
    DaiObjModule* module  = create_test_module(&vm);
    DaiObjFunction* func1 = DaiObjFunction_New(&vm, module, "<test1>", "<test>");
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
    DaiObjFunction* func2 = DaiObjFunction_New(&vm, module, "<test1>", "<test>");
    {
        uint8_t expected_codes1[] = {
            DaiOpReturn,
        };
        for (int i = 0; i < sizeof(expected_codes1) / sizeof(expected_codes1[0]); i++) {
            DaiChunk_write(&func2->chunk, expected_codes1[i], 1);
        }
    }
    DaiObjFunction* func3 = DaiObjFunction_New(&vm, module, "<test1>", "<test>");
    {
        uint8_t expected_codes1[] = {
            DaiOpReturn,
        };
        for (int i = 0; i < sizeof(expected_codes1) / sizeof(expected_codes1[0]); i++) {
            DaiChunk_write(&func3->chunk, expected_codes1[i], 1);
        }
    }
    DaiObjFunction* func4 = DaiObjFunction_New(&vm, module, "<test1>", "<test>");
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
    DaiObjFunction* func5 = DaiObjFunction_New(&vm, module, "<test1>", "<test>");
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
            6 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpCall,
                0,
                DaiOpPop,
                DaiOpEnd,
            },
            {
                INTEGER_VAL(24),
                OBJ_VAL(func1),
            },
        },
        {
            "var noArg = fn() {24;};\n noArg();",
            12 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpGetGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpCall,
                0,
                DaiOpPop,
                DaiOpEnd,
            },
            {
                INTEGER_VAL(24),
                OBJ_VAL(func1),
            },
        },
        {
            "var oneArg = fn(a) {};\n oneArg(24);",
            15 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpGetGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpConstant,
                0,
                1,
                DaiOpCall,
                1,
                DaiOpPop,
                DaiOpEnd,
            },
            {
                OBJ_VAL(func2),
                INTEGER_VAL(24),
            },
        },
        {
            "var manyArg = fn(a, b, c) {};\n manyArg(24, 25, 26);",
            21 + 1,
            {
                DaiOpConstant,     0, 0,
                DaiOpDefineGlobal, 0, 0 + BUILTIN_GLOBALS_COUNT,
                DaiOpGetGlobal,    0, 0 + BUILTIN_GLOBALS_COUNT,
                DaiOpConstant,     0, 1,
                DaiOpConstant,     0, 2,
                DaiOpConstant,     0, 3,
                DaiOpCall,         3, DaiOpPop,
                DaiOpEnd,
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
            15 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpGetGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpConstant,
                0,
                1,
                DaiOpCall,
                1,
                DaiOpPop,
                DaiOpEnd,
            },
            {
                OBJ_VAL(func4),
                INTEGER_VAL(24),
            },
        },
        {
            "var manyArg = fn(a, b, c) { a; b; c;};\n manyArg(24, 25, 26);",
            21 + 1,
            {
                DaiOpConstant,     0, 0,
                DaiOpDefineGlobal, 0, 0 + BUILTIN_GLOBALS_COUNT,
                DaiOpGetGlobal,    0, 0 + BUILTIN_GLOBALS_COUNT,
                DaiOpConstant,     0, 1,
                DaiOpConstant,     0, 2,
                DaiOpConstant,     0, 3,
                DaiOpCall,         3, DaiOpPop,
                DaiOpEnd,
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
test_var_statement(__attribute__((unused)) const MunitParameter params[],
                   __attribute__((unused)) void* user_data) {
    DaiVM vm;
    DaiVM_init(&vm);
    DaiObjModule* module  = create_test_module(&vm);
    DaiObjFunction* func1 = DaiObjFunction_New(&vm, module, "<test1>", "<test>");
    {
        uint8_t expected_codes1[] = {
            DaiOpGetGlobal,
            0,
            0 + BUILTIN_GLOBALS_COUNT,
            DaiOpPop,
            DaiOpReturn,
        };
        for (int i = 0; i < sizeof(expected_codes1) / sizeof(expected_codes1[0]); i++) {
            DaiChunk_write(&func1->chunk, expected_codes1[i], 1);
        }
    }
    DaiObjFunction* func2 = DaiObjFunction_New(&vm, module, "<test2>", "<test>");
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
    DaiObjFunction* func3 = DaiObjFunction_New(&vm, module, "<test3>", "<test>");
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
            10 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpConstant,
                0,
                1,
                DaiOpPop,
                DaiOpEnd,
            },
            {
                INTEGER_VAL(55),
                OBJ_VAL(func1),
            },
        },
        {
            "fn() {var num = 55; num;};",
            4 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpPop,
                DaiOpEnd,
            },
            {
                INTEGER_VAL(55),
                OBJ_VAL(func2),
            },
        },
        {
            "fn() { var a = 55; var b = 77; a + b; };",
            4 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpPop,
                DaiOpEnd,
            },
            {
                INTEGER_VAL(55),
                INTEGER_VAL(77),
                OBJ_VAL(func3),
            },
        },
        {
            "con num = 55; fn() {num;};",
            10 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpConstant,
                0,
                1,
                DaiOpPop,
                DaiOpEnd,
            },
            {
                INTEGER_VAL(55),
                OBJ_VAL(func1),
            },
        },
        {
            "con num = 3; fn() {var num = 55; num;};",
            10 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpConstant,
                0,
                1,
                DaiOpPop,
                DaiOpEnd,
            },
            {
                INTEGER_VAL(3),
                INTEGER_VAL(55),
                OBJ_VAL(func2),
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
    DaiObjModule* module = create_test_module(&vm);
    DaiObjString s = (DaiObjString){{.type = DaiObjType_string}, .chars = "monkey", .length = 6};
    DaiObjFunction* func = DaiObjFunction_New(&vm, module, "<test2>", "<test>");
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
            8 + 1,
            {
                DaiOpGetBuiltin,
                1,
                DaiOpConstant,
                0,
                0,
                DaiOpCall,
                1,
                DaiOpPop,
                DaiOpEnd,
            },
            {
                OBJ_VAL(&s),
            },
        },
        {
            "fn() {len(\"monkey\");};",
            4 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpPop,
                DaiOpEnd,
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
    DaiObjModule* module    = create_test_module(&vm);
    DaiObjFunction* func1_1 = DaiObjFunction_New(&vm, module, "<test1_1>", "<test>");
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
    DaiObjFunction* func1_2 = DaiObjFunction_New(&vm, module, "<test1_2>", "<test>");
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
    DaiObjFunction* func2_1 = DaiObjFunction_New(&vm, module, "<test2>", "<test>");
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
    DaiObjFunction* func2_2 = DaiObjFunction_New(&vm, module, "<test2>", "<test>");
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
    DaiObjFunction* func2_3 = DaiObjFunction_New(&vm, module, "<test2>", "<test>");
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
    DaiObjFunction* func3_1 = DaiObjFunction_New(&vm, module, "<test3_1>", "<test>");
    {
        uint8_t expected_codes2[] = {
            DaiOpConstant,
            0,
            0,
            DaiOpSetLocal,
            1,
            DaiOpGetGlobal,
            0,
            0 + BUILTIN_GLOBALS_COUNT,
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
    DaiObjFunction* func3_2 = DaiObjFunction_New(&vm, module, "<test3_2>", "<test>");
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
    DaiObjFunction* func3_3 = DaiObjFunction_New(&vm, module, "<test3_3>", "<test>");
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
            4 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpPop,
                DaiOpEnd,
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
            4 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpPop,
                DaiOpEnd,
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
            10 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpConstant,
                0,
                1,
                DaiOpPop,
                DaiOpEnd,
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
    DaiObjModule* module = create_test_module(&vm);
    DaiObjFunction* func = DaiObjFunction_New(&vm, module, "s4", "<test>");
    {
        uint8_t expected_codes2[] = {
            DaiOpReturn,
        };
        for (int i = 0; i < sizeof(expected_codes2) / sizeof(expected_codes2[0]); i++) {
            DaiChunk_write(&func->chunk, expected_codes2[i], 1);
        }
    }
    DaiObjFunction* func2 = DaiObjFunction_New(&vm, module, "func2", "<test>");
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
            // class;
            DaiOpGetLocal,
            0,
            DaiOpPop,
            // class.classVar;
            DaiOpGetSelfProperty,
            0,
            0,
            DaiOpPop,
            // class.classVar = 5;
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
            0 + BUILTIN_GLOBALS_COUNT,
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
    DaiObjFunction* func2_2 = DaiObjFunction_New(&vm, module, "func2_2", "<test>");
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
            // class;
            DaiOpGetLocal,
            0,
            DaiOpPop,
            // class.classVar;
            DaiOpGetSelfProperty,
            0,
            0,
            DaiOpPop,
            // class.classVar = 5;
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
            1 + BUILTIN_GLOBALS_COUNT,
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
            10 + 1,
            {
                DaiOpClass,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpGetGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpPop,
                DaiOpEnd,
            },
            {
                OBJ_VAL(&s),
            },
        },
        {
            "class C {\n"
            "  var s2;\n"
            "};\n",
            15 + 1,
            {
                DaiOpClass,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpGetGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpUndefined,
                DaiOpDefineField,
                0,
                1,
                0,
                DaiOpPop,
                DaiOpEnd,
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
            22 + 1,
            {
                DaiOpClass,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpGetGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,

                DaiOpUndefined,
                DaiOpDefineField,
                0,
                1,
                0,

                DaiOpConstant,
                0,
                2,
                DaiOpDefineField,
                0,
                3,
                0,

                DaiOpPop,
                DaiOpEnd,
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
            28 + 1,
            {
                DaiOpClass,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpGetGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,

                DaiOpUndefined,
                DaiOpDefineField,
                0,
                1,
                0,

                DaiOpConstant,
                0,
                2,
                DaiOpDefineField,
                0,
                3,
                0,

                DaiOpConstant,
                0,
                4,
                DaiOpDefineMethod,
                0,
                5,

                DaiOpPop,
                DaiOpEnd,
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
            35 + 1,
            {
                DaiOpClass,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpGetGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,

                DaiOpUndefined,
                DaiOpDefineField,
                0,
                1,
                0,

                DaiOpConstant,
                0,
                2,
                DaiOpDefineField,
                0,
                3,
                0,

                DaiOpConstant,
                0,
                4,
                DaiOpDefineMethod,
                0,
                5,

                DaiOpConstant,
                0,
                6,
                DaiOpDefineClassField,
                0,
                7,
                0,
                DaiOpPop,
                DaiOpEnd,
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
            "    class;\n"
            "    class.classVar;\n"
            "    class.classVar = 5;\n"
            "    super.func2;\n"
            "    var c = C();\n"
            "    c.s2 = 6;\n"
            "    c.s3;\n"
            "  };\n"
            "};\n",
            41 + 1,
            {
                DaiOpClass,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpGetGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,

                DaiOpUndefined,
                DaiOpDefineField,
                0,
                1,
                0,

                DaiOpConstant,
                0,
                2,
                DaiOpDefineField,
                0,
                3,
                0,

                DaiOpConstant,
                0,
                4,
                DaiOpDefineMethod,
                0,
                5,

                DaiOpConstant,
                0,
                6,
                DaiOpDefineClassField,
                0,
                7,
                0,

                DaiOpConstant,
                0,
                8,
                DaiOpDefineClassMethod,
                0,
                9,
                DaiOpPop,
                DaiOpEnd,
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
            "class C(B) {\n"
            "  var s2;\n"
            "  var s3 = 0;\n"
            "  fn s4() {};\n"
            "  class var classVar = 3;\n"
            "  class fn func2_2() {\n"
            "    class;\n"
            "    class.classVar;\n"
            "    class.classVar = 5;\n"
            "    super.func2_2;\n"
            "    var c = C();\n"
            "    c.s2 = 6;\n"
            "    c.s3;\n"
            "  };\n"
            "};\n",
            55 + 1,
            {
                DaiOpClass,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpGetGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpPop,

                DaiOpClass,
                0,
                1,
                DaiOpGetGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpInherit,
                DaiOpDefineGlobal,
                0,
                1 + BUILTIN_GLOBALS_COUNT,
                DaiOpGetGlobal,
                0,
                1 + BUILTIN_GLOBALS_COUNT,

                DaiOpUndefined,
                DaiOpDefineField,
                0,
                2,
                0,

                DaiOpConstant,
                0,
                3,
                DaiOpDefineField,
                0,
                4,
                0,

                DaiOpConstant,
                0,
                5,
                DaiOpDefineMethod,
                0,
                6,

                DaiOpConstant,
                0,
                7,
                DaiOpDefineClassField,
                0,
                8,
                0,

                DaiOpConstant,
                0,
                9,
                DaiOpDefineClassMethod,
                0,
                10,
                DaiOpPop,
                DaiOpEnd,
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
            "CompileError: symbol 'a' already defined in <test-file>:2:5",
        },
        {
            "a;",
            "CompileError: undefined variable: 'a' in <test-file>:1:1",
        },
        {
            "a = 1;",
            "CompileError: undefined variable: 'a' in <test-file>:1:1",
        },
        {
            "a; var a = 1;",
            "CompileError: undefined variable: 'a' in <test-file>:1:1",
        },
        {
            "class Foo {var a; fn a() {};};",
            "CompileError: property 'a' already defined in <test-file>:1:19",
        },
        {
            "con a = 1; a = 2;",
            "CompileError: cannot assign to const variable 'a' in <test-file>:1:12",
        },
    };
    for (int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        DaiVM vm;
        DaiVM_init(&vm);
        DaiObjModule* module = create_test_module(&vm);
        DaiCompileError* err = compile_error_helper(tests[i].input, module, &vm);
        munit_assert_not_null(err);
        char* msg = DaiCompileError_string(err);
        munit_assert_string_equal(msg, tests[i].expected_error_msg);
        free(msg);
        DaiCompileError_free(err);
        DaiVM_reset(&vm);
    }
    return MUNIT_OK;
}

static MunitResult
test_array(__attribute__((unused)) const MunitParameter params[],
           __attribute__((unused)) void* user_data) {
    const DaiCompilerTestCase tests[] = {
        {
            "[];",
            4 + 1,
            {
                DaiOpArray,
                0,
                0,
                DaiOpPop,
                DaiOpEnd,
            },
            {},
        },
        {
            "[1];",
            7 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpArray,
                0,
                1,
                DaiOpPop,
                DaiOpEnd,
            },
            {
                INTEGER_VAL(1),
            },
        },
        {
            "[1, 23];",
            10 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpConstant,
                0,
                1,
                DaiOpArray,
                0,
                2,
                DaiOpPop,
                DaiOpEnd,
            },
            {
                INTEGER_VAL(1),
                INTEGER_VAL(23),
            },
        },
        {
            "[1, 23, 1 + 2];",
            17 + 1,
            {
                DaiOpConstant,
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
                DaiOpAdd,

                DaiOpArray,
                0,
                3,
                DaiOpPop,
                DaiOpEnd,
            },
            {
                INTEGER_VAL(1),
                INTEGER_VAL(23),
                INTEGER_VAL(1),
                INTEGER_VAL(2),
            },
        },

    };
    run_compiler_tests(tests, sizeof(tests) / sizeof(tests[0]));
    return MUNIT_OK;
}

static MunitResult
test_subscript_expression(__attribute__((unused)) const MunitParameter params[],
                          __attribute__((unused)) void* user_data) {
    const DaiCompilerTestCase tests[] = {
        {
            "[][1];",
            8 + 1,
            {
                DaiOpArray,
                0,
                0,
                DaiOpConstant,
                0,
                0,
                DaiOpSubscript,
                DaiOpPop,
                DaiOpEnd,
            },
            {INTEGER_VAL(1)},
        },
        {
            "[0, 1, 2][0];",
            17 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpConstant,
                0,
                1,
                DaiOpConstant,
                0,
                2,
                DaiOpArray,
                0,
                3,
                DaiOpConstant,
                0,
                3,
                DaiOpSubscript,
                DaiOpPop,
                DaiOpEnd,
            },
            {
                INTEGER_VAL(0),
                INTEGER_VAL(1),
                INTEGER_VAL(2),
                INTEGER_VAL(0),
            },
        },

        {
            "[1, 2, 3][1 + 1];",
            21 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpConstant,
                0,
                1,
                DaiOpConstant,
                0,
                2,
                DaiOpArray,
                0,
                3,
                DaiOpConstant,
                0,
                3,
                DaiOpConstant,
                0,
                4,
                DaiOpAdd,
                DaiOpSubscript,
                DaiOpPop,
                DaiOpEnd,
            },
            {
                INTEGER_VAL(1),
                INTEGER_VAL(2),
                INTEGER_VAL(3),
                INTEGER_VAL(1),
                INTEGER_VAL(1),
            },
        },
        {
            "[1, 2, 3][1 - 1];",
            21 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpConstant,
                0,
                1,
                DaiOpConstant,
                0,
                2,
                DaiOpArray,
                0,
                3,
                DaiOpConstant,
                0,
                3,
                DaiOpConstant,
                0,
                4,
                DaiOpSub,
                DaiOpSubscript,
                DaiOpPop,
                DaiOpEnd,
            },
            {
                INTEGER_VAL(1),
                INTEGER_VAL(2),
                INTEGER_VAL(3),
                INTEGER_VAL(1),
                INTEGER_VAL(1),
            },
        },

        {
            "var m = {}; m[0];",
            14 + 1,
            {
                DaiOpMap,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpGetGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpConstant,
                0,
                0,
                DaiOpSubscript,
                DaiOpPop,
                DaiOpEnd,
            },
            {INTEGER_VAL(0)},
        },

    };
    run_compiler_tests(tests, sizeof(tests) / sizeof(tests[0]));
    return MUNIT_OK;
}

static MunitResult
test_subscript_set_expression(__attribute__((unused)) const MunitParameter params[],
                              __attribute__((unused)) void* user_data) {
    const DaiCompilerTestCase tests[] = {
        {
            "var m = []; m[0] = 1;",
            16 + 1,
            {
                DaiOpArray,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpConstant,
                0,
                0,
                DaiOpGetGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpConstant,
                0,
                1,
                DaiOpSubscriptSet,
                DaiOpEnd,
            },
            {INTEGER_VAL(1), INTEGER_VAL(0)},
        },
        {
            "var m = []; var n = 0; m[n] = 1;",
            22 + 1,
            {
                // var m = [];
                DaiOpArray,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                // var n = 0;
                DaiOpConstant,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                1 + BUILTIN_GLOBALS_COUNT,
                // m[n] = 1;
                DaiOpConstant,
                0,
                1,
                DaiOpGetGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpGetGlobal,
                0,
                1 + BUILTIN_GLOBALS_COUNT,
                DaiOpSubscriptSet,
                DaiOpEnd,
            },
            {INTEGER_VAL(0), INTEGER_VAL(1)},
        },


    };
    run_compiler_tests(tests, sizeof(tests) / sizeof(tests[0]));
    return MUNIT_OK;
}

static MunitResult
test_block_statement(__attribute__((unused)) const MunitParameter params[],
                     __attribute__((unused)) void* user_data) {
    const DaiCompilerTestCase tests[] = {
        {
            "{1;};",
            4 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpPop,
                DaiOpEnd,
            },
            {INTEGER_VAL(1)},
        },
        {
            "{1; 2;};",
            8 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpPop,
                DaiOpConstant,
                0,
                1,
                DaiOpPop,
                DaiOpEnd,
            },
            {INTEGER_VAL(1), INTEGER_VAL(2)},
        },
        {
            "{1; 2; 3;};",
            12 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpPop,
                DaiOpConstant,
                0,
                1,
                DaiOpPop,
                DaiOpConstant,
                0,
                2,
                DaiOpPop,
                DaiOpEnd,
            },
            {INTEGER_VAL(1), INTEGER_VAL(2), INTEGER_VAL(3)},
        },
    };
    run_compiler_tests(tests, sizeof(tests) / sizeof(tests[0]));
    return MUNIT_OK;
}

static MunitResult
test_map(__attribute__((unused)) const MunitParameter params[],
         __attribute__((unused)) void* user_data) {
    const DaiCompilerTestCase tests[] = {
        {
            "var m = {};",
            6 + 1,
            {
                DaiOpMap,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpEnd,
            },
            {},
        },
        {
            "var m = {1: 2};",
            12 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpConstant,
                0,
                1,
                DaiOpMap,
                0,
                1,
                DaiOpDefineGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpEnd,
            },
            {
                INTEGER_VAL(1),
                INTEGER_VAL(2),
            },
        },
        {
            "var m = {1: 2, 3: 4};",
            18 + 1,
            {
                DaiOpConstant, 0, 0, DaiOpConstant,     0, 1,
                DaiOpConstant, 0, 2, DaiOpConstant,     0, 3,
                DaiOpMap,      0, 2, DaiOpDefineGlobal, 0, 0 + BUILTIN_GLOBALS_COUNT,
                DaiOpEnd,
            },
            {
                INTEGER_VAL(1),
                INTEGER_VAL(2),
                INTEGER_VAL(3),
                INTEGER_VAL(4),
            },
        },
        {
            "var m = {1: 2, 3: 4, 5: 6};",
            24 + 1,
            {
                DaiOpConstant, 0, 0, DaiOpConstant,     0, 1,
                DaiOpConstant, 0, 2, DaiOpConstant,     0, 3,
                DaiOpConstant, 0, 4, DaiOpConstant,     0, 5,
                DaiOpMap,      0, 3, DaiOpDefineGlobal, 0, 0 + BUILTIN_GLOBALS_COUNT,
                DaiOpEnd,
            },
            {
                INTEGER_VAL(1),
                INTEGER_VAL(2),
                INTEGER_VAL(3),
                INTEGER_VAL(4),
                INTEGER_VAL(5),
                INTEGER_VAL(6),
            },
        },
    };
    run_compiler_tests(tests, sizeof(tests) / sizeof(tests[0]));
    return MUNIT_OK;
}

static MunitResult
test_assign_statement(__attribute__((unused)) const MunitParameter params[],
                      __attribute__((unused)) void* user_data) {
    const DaiCompilerTestCase tests[] = {
        {
            "var a = 1; a += 1;",
            16 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpGetGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpConstant,
                0,
                1,
                DaiOpAdd,
                DaiOpSetGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpEnd,
            },
            {
                INTEGER_VAL(1),
                INTEGER_VAL(1),
            },
        },
        {
            "var a = 1; a -= 1;",
            16 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpGetGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpConstant,
                0,
                1,
                DaiOpSub,
                DaiOpSetGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpEnd,
            },
            {
                INTEGER_VAL(1),
                INTEGER_VAL(1),
            },
        },
        {
            "var a = 1; a *= 1;",
            16 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpGetGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpConstant,
                0,
                1,
                DaiOpMul,
                DaiOpSetGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpEnd,
            },
            {
                INTEGER_VAL(1),
                INTEGER_VAL(1),
            },
        },
        {
            "var a = 1; a /= 1;",
            16 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpDefineGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpGetGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpConstant,
                0,
                1,
                DaiOpDiv,
                DaiOpSetGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpEnd,
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
test_forin_statement(__attribute__((unused)) const MunitParameter params[],
                     __attribute__((unused)) void* user_data) {
    const DaiCompilerTestCase tests[] = {
        {
            "var a = [1, 2, 3]; for (var i, e in a) {};",
            27 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpConstant,
                0,
                1,
                DaiOpConstant,
                0,
                2,
                DaiOpArray,
                0,
                3,
                DaiOpDefineGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                // for (var i, e in a) {};
                DaiOpGetGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpIterInit,
                0,
                DaiOpIterNext,
                0,
                0,
                3,
                DaiOpJumpBack,
                0,
                7,
                DaiOpEnd,

            },
            {
                INTEGER_VAL(1),
                INTEGER_VAL(2),
                INTEGER_VAL(3),
            },
        },
        {
            "var a = [1, 2, 3]; for (var i, e in a) { break; continue; };",
            33 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpConstant,
                0,
                1,
                DaiOpConstant,
                0,
                2,
                DaiOpArray,
                0,
                3,
                DaiOpDefineGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                // for (var i, e in a) {};
                DaiOpGetGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpIterInit,
                0,
                DaiOpIterNext,
                0,
                0,
                9,
                // break;
                DaiOpJump,
                0,
                6,
                // continue
                DaiOpJumpBack,
                0,
                10,
                DaiOpJumpBack,
                0,
                13,
                DaiOpEnd,
            },
            {
                INTEGER_VAL(1),
                INTEGER_VAL(2),
                INTEGER_VAL(3),
            },
        },
        {
            "var a = [1, 2, 3]; for (var e in a) { break; continue; };",
            33 + 1,
            {
                DaiOpConstant,
                0,
                0,
                DaiOpConstant,
                0,
                1,
                DaiOpConstant,
                0,
                2,
                DaiOpArray,
                0,
                3,
                DaiOpDefineGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                // for (var i, e in a) {};
                DaiOpGetGlobal,
                0,
                0 + BUILTIN_GLOBALS_COUNT,
                DaiOpIterInit,
                0,
                DaiOpIterNext,
                0,
                0,
                9,
                // break;
                DaiOpJump,
                0,
                6,
                // continue
                DaiOpJumpBack,
                0,
                10,
                DaiOpJumpBack,
                0,
                13,
                DaiOpEnd,
            },
            {
                INTEGER_VAL(1),
                INTEGER_VAL(2),
                INTEGER_VAL(3),
            },
        },

    };
    run_compiler_tests(tests, sizeof(tests) / sizeof(tests[0]));
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
    {(char*)"/test_var_statement", test_var_statement, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/test_builtins", test_builtins, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/test_closures", test_closures, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/test_class", test_class, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/test_compile_error", test_compile_error, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/test_array", test_array, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/test_subscript_expression",
     test_subscript_expression,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/test_subscript_set_expression",
     test_subscript_set_expression,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/test_block_statement",
     test_block_statement,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/test_map", test_map, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/test_assign_statement",
     test_assign_statement,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/test_forin_statement",
     test_forin_statement,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};
