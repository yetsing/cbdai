#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>

#include "munit/munit.h"
#include "cwalk.h"

#include "dai.h"
#include "dai_windows.h"


// 获取当前文件所在文件夹路径
static void
get_file_directory(char* path) {
    if (realpath(__FILE__, path) == NULL) {
        perror("realpath");
        assert(false);
    }
    size_t len = 0;
    cwk_path_get_dirname(path, &len);
    path[len] = '\0';
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
    daicall_push_function(dai, sum_int);
    daicall_pusharg_int(dai, 1);
    daicall_pusharg_int(dai, 2);
    daicall_execute(dai);
    munit_assert_int64(daicall_getrv_int(dai), ==, 3);

    // Test sum_float function
    daicall_push_function(dai, sum_float);
    daicall_pusharg_float(dai, 1.1);
    daicall_pusharg_float(dai, 2.3);
    daicall_execute(dai);
    munit_assert_double(daicall_getrv_float(dai), ==, 3.4);

    // Test sum_string function
    daicall_push_function(dai, sum_string);
    daicall_pusharg_string(dai, "hello");
    daicall_pusharg_string(dai, "world");
    daicall_execute(dai);
    munit_assert_string_equal(daicall_getrv_string(dai), "helloworld");

    dai_free(dai);
    return MUNIT_OK;
}

static void
add_int(Dai* dai) {
    int64_t a = daicall_poparg_int(dai);
    int64_t b = daicall_poparg_int(dai);
    daicall_setrv_int(dai, a + b);
}

static void
add_float(Dai* dai) {
    double a = daicall_poparg_float(dai);
    double b = daicall_poparg_float(dai);
    daicall_setrv_float(dai, a + b);
}

static void
add_string(Dai* dai) {
    const char* a = daicall_poparg_string(dai);
    const char* b = daicall_poparg_string(dai);
    char* result  = malloc(strlen(a) + strlen(b) + 1);
    strcpy(result, a);
    strcat(result, b);
    daicall_setrv_string(dai, result);
    free(result);
}

static void
return_nil(Dai* dai) {
    daicall_setrv_nil(dai);
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
