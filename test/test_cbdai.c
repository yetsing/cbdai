#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>

#include "munit/munit.h"

#include "dai.h"


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

static MunitResult
test_dai_variable(__attribute__((unused)) const MunitParameter params[],
                  __attribute__((unused)) void* user_data) {
    char resolved_path[PATH_MAX];
    get_file_directory(resolved_path);
    strcat(resolved_path, "dai_variable_example.dai");
    Dai* dai = dai_new();
    dai_load_file(dai, resolved_path);
    {
        munit_assert_int64(dai_get_int(dai, "intv"), ==, 1);
        munit_assert_double(dai_get_float(dai, "floatv"), ==, 1.1);
        munit_assert_string_equal(dai_get_string(dai, "strv"), "hello");

        dai_set_int(dai, "intv", 2);
        dai_set_float(dai, "floatv", 2.2);
        dai_set_string(dai, "strv", "world");

        munit_assert_int64(dai_get_int(dai, "intv"), ==, 2);
        munit_assert_double(dai_get_float(dai, "floatv"), ==, 2.2);
        munit_assert_string_equal(dai_get_string(dai, "strv"), "world");
    }
    dai_free(dai);
    return MUNIT_OK;
}

static MunitResult
test_dai_call(__attribute__((unused)) const MunitParameter params[],
              __attribute__((unused)) void* user_data) {
    char resolved_path[PATH_MAX];
    get_file_directory(resolved_path);
    strcat(resolved_path, "dai_call_example.dai");
    Dai* dai = dai_new();
    dai_load_file(dai, resolved_path);
    dai_func_t sum_int    = dai_get_function(dai, "sum_int");
    dai_func_t sum_float  = dai_get_function(dai, "sum_float");
    dai_func_t sum_string = dai_get_function(dai, "sum_string");
    // implement test
    // Test sum_int function
    dai_call_push_function(dai, sum_int);
    dai_call_push_arg_int(dai, 1);
    dai_call_push_arg_int(dai, 2);
    dai_call_execute(dai);
    munit_assert_int64(dai_call_return_int(dai), ==, 3);

    // Test sum_float function
    dai_call_push_function(dai, sum_float);
    dai_call_push_arg_float(dai, 1.1);
    dai_call_push_arg_float(dai, 2.3);
    dai_call_execute(dai);
    munit_assert_double(dai_call_return_float(dai), ==, 3.4);

    // Test sum_string function
    dai_call_push_function(dai, sum_string);
    dai_call_push_arg_string(dai, "hello");
    dai_call_push_arg_string(dai, "world");
    dai_call_execute(dai);
    munit_assert_string_equal(dai_call_return_string(dai), "helloworld");

    dai_free(dai);
    return MUNIT_OK;
}

static void
add_int(Dai* dai) {
    int64_t a = dai_call_pop_arg_int(dai);
    int64_t b = dai_call_pop_arg_int(dai);
    dai_call_push_return_int(dai, a + b);
}

static void
add_float(Dai* dai) {
    double a = dai_call_pop_arg_float(dai);
    double b = dai_call_pop_arg_float(dai);
    dai_call_push_return_float(dai, a + b);
}

static void
add_string(Dai* dai) {
    const char* a = dai_call_pop_arg_string(dai);
    const char* b = dai_call_pop_arg_string(dai);
    char* result  = malloc(strlen(a) + strlen(b) + 1);
    strcpy(result, a);
    strcat(result, b);
    dai_call_push_return_string(dai, result);
    free(result);
}

static void
return_nil(Dai* dai) {
    dai_call_push_return_nil(dai);
}

static MunitResult
test_dai_c_function(__attribute__((unused)) const MunitParameter params[],
                    __attribute__((unused)) void* user_data) {
    char resolved_path[PATH_MAX];
    get_file_directory(resolved_path);
    strcat(resolved_path, "dai_c_function_example.dai");
    Dai* dai = dai_new();
    dai_register_function(dai, "add_int", add_int, 2);
    dai_register_function(dai, "add_float", add_float, 2);
    dai_register_function(dai, "add_string", add_string, 2);
    dai_register_function(dai, "return_nil", return_nil, 0);
    dai_load_file(dai, resolved_path);
    dai_free(dai);
    return MUNIT_OK;
}

MunitTest cbdai_tests[] = {
    {(char*)"/test_dai_variable", test_dai_variable, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/test_dai_call", test_dai_call, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/test_dai_c_function", test_dai_c_function, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};
