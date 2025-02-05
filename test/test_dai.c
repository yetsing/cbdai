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
test_dai(__attribute__((unused)) const MunitParameter params[],
         __attribute__((unused)) void* user_data) {
    char resolved_path[PATH_MAX];
    get_file_directory(resolved_path);
    strcat(resolved_path, "dai_example.dai");
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

MunitTest dai_tests[] = {
    {(char*)"/test_dai", test_dai, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};
