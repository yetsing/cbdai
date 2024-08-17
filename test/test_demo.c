#include "munit/munit.h"

static MunitResult
test_demo(__attribute__((unused)) const MunitParameter params[],
          __attribute__((unused)) void*                user_data) {
    return MUNIT_OK;
}

MunitTest demo_tests[] = {
    {(char*)"/test_demo", test_demo, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};
