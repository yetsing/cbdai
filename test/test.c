#include <stdio.h>

#include <munit/munit.h>

#include "test.h"

extern MunitTest demo_tests[];
extern MunitTest atstr_tests[];
extern MunitTest tokenize_suite_tests[];
extern MunitTest stringbuffer_tests[];
extern MunitTest parse_tests[];
extern MunitTest parseint_tests[];
extern MunitTest ast_tests[];
extern MunitTest codecs_tests[];
extern MunitTest chunk_tests[];
extern MunitTest compile_tests[];
extern MunitTest vm_tests[];
extern MunitTest table_tests[];
extern MunitTest symboltable_tests[];
extern MunitTest cbdai_tests[];

static MunitSuite sub_suites[] = {
    {"/demo", demo_tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
    {"/atstr", atstr_tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
    {"/tokenize", tokenize_suite_tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
    {"/stringbuffer", stringbuffer_tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
    {"/parse", parse_tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
    {"/parseint", parseint_tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
    {"/ast", ast_tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
    {"/codecs", codecs_tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
    {"/chunk", chunk_tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
    {"/compile", compile_tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
    {"/vm", vm_tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
    {"/table", table_tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
    {"/symboltable", symboltable_tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
    {"/cbdai", cbdai_tests, NULL, 1, MUNIT_SUITE_OPTION_NONE},
    {NULL, NULL, NULL, 0, MUNIT_SUITE_OPTION_NONE},
};

static const MunitSuite dai_suite = {
    "/dai",                 /* name */
    NULL,                   /* tests */
    sub_suites,             /* suites */
    1,                      /* iterations */
    MUNIT_SUITE_OPTION_NONE /* options */
};

int
test(int argc, const char* argv[]) {
    return munit_suite_main(&dai_suite, NULL, argc, (char* const*)argv);
}

int
main(int argc, const char* argv[]) {
    return test(argc, argv);
}