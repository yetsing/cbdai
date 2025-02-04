#include <stdbool.h>
#include <stdio.h>

#include "munit/munit.h"

#include "dai_symboltable.h"

static MunitResult
test_define(__attribute__((unused)) const MunitParameter params[],
            __attribute__((unused)) void* user_data) {
    struct {
        const char* name;
        DaiSymbol symbol;
    } tests[] = {
        {
            "a",
            {
                .name  = "a",
                .depth = 0,
                .index = 0,
            },
        },
        {
            "b",
            {
                .name  = "b",
                .depth = 0,
                .index = 1,
            },
        },
        {
            "c",
            {
                .name  = "c",
                .depth = 1,
                .index = 0,
            },
        },
        {
            "d",
            {
                .name  = "d",
                .depth = 1,
                .index = 1,
            },
        },
        {
            "e",
            {
                .name  = "e",
                .depth = 2,
                .index = 2,
            },
        },
        {
            "f",
            {
                .name  = "f",
                .depth = 2,
                .index = 3,
            },
        },
    };

    DaiSymbolTable* global = DaiSymbolTable_New();

    for (int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        if (tests[i].symbol.depth == 0) {
            DaiSymbol got = DaiSymbolTable_define(global, tests[i].name, false);
            munit_assert_string_equal(got.name, tests[i].name);
            munit_assert_int(got.depth, ==, tests[i].symbol.depth);
            munit_assert_int(got.index, ==, tests[i].symbol.index);
        }
    }
    DaiSymbolTable* first_local = DaiSymbolTable_NewEnclosed(global);
    for (int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        if (tests[i].symbol.depth == 1) {
            DaiSymbol got = DaiSymbolTable_define(first_local, tests[i].name, false);
            munit_assert_string_equal(got.name, tests[i].name);
            munit_assert_int(got.depth, ==, tests[i].symbol.depth);
            munit_assert_int(got.index, ==, tests[i].symbol.index);
        }
    }
    DaiSymbolTable* second_local = DaiSymbolTable_NewEnclosed(first_local);
    for (int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        if (tests[i].symbol.depth == 2) {
            DaiSymbol got = DaiSymbolTable_define(second_local, tests[i].name, false);
            munit_assert_string_equal(got.name, tests[i].name);
            munit_assert_int(got.depth, ==, tests[i].symbol.depth);
            munit_assert_int(got.index, ==, tests[i].symbol.index);
        }
    }

    DaiSymbolTable_free(second_local);
    DaiSymbolTable_free(first_local);
    DaiSymbolTable_free(global);
    return MUNIT_OK;
}

static MunitResult
test_resolve_global(__attribute__((unused)) const MunitParameter params[],
                    __attribute__((unused)) void* user_data) {
    DaiSymbolTable* table = DaiSymbolTable_New();
    DaiSymbolTable_define(table, "a", false);
    DaiSymbolTable_define(table, "b", false);

    DaiSymbol expected[] = {
        {
            .name  = "a",
            .depth = 0,
            .index = 0,
        },
        {
            .name  = "b",
            .depth = 0,
            .index = 1,
        },
    };

    for (int i = 0; i < sizeof(expected) / sizeof(expected[0]); i++) {
        DaiSymbol got;
        bool found = DaiSymbolTable_resolve(table, expected[i].name, &got);
        munit_assert_true(found);
        munit_assert_string_equal(got.name, expected[i].name);
        munit_assert_int(got.depth, ==, expected[i].depth);
        munit_assert_int(got.index, ==, expected[i].index);
    }

    DaiSymbolTable_free(table);
    return MUNIT_OK;
}

static MunitResult
test_resolve_local(__attribute__((unused)) const MunitParameter params[],
                   __attribute__((unused)) void* user_data) {
    DaiSymbolTable* global = DaiSymbolTable_New();
    DaiSymbolTable_define(global, "a", false);
    DaiSymbolTable_define(global, "b", false);
    DaiSymbolTable* local = DaiSymbolTable_NewEnclosed(global);
    DaiSymbolTable_define(local, "c", false);
    DaiSymbolTable_define(local, "d", false);
    DaiSymbolTable* secondLocal = DaiSymbolTable_NewEnclosed(local);
    DaiSymbolTable_define(secondLocal, "e", false);
    DaiSymbolTable_define(secondLocal, "f", false);

    DaiSymbol expected[] = {
        {
            .name  = "a",
            .depth = 0,
            .index = 0,
        },
        {
            .name  = "b",
            .depth = 0,
            .index = 1,
        },
        {
            .name  = "c",
            .depth = 1,
            .index = 0,
        },
        {
            .name  = "d",
            .depth = 1,
            .index = 1,
        },
        {
            .name  = "e",
            .depth = 2,
            .index = 2,
        },
        {
            .name  = "f",
            .depth = 2,
            .index = 3,
        },
    };

    for (int i = 0; i < sizeof(expected) / sizeof(expected[0]); i++) {
        DaiSymbol got;
        bool found = DaiSymbolTable_resolve(secondLocal, expected[i].name, &got);
        munit_assert_true(found);
        munit_assert_string_equal(got.name, expected[i].name);
        munit_assert_int(got.depth, ==, expected[i].depth);
        munit_assert_int(got.index, ==, expected[i].index);
    }

    DaiSymbolTable_free(secondLocal);
    DaiSymbolTable_free(local);
    DaiSymbolTable_free(global);
    return MUNIT_OK;
}

MunitTest symboltable_tests[] = {
    {(char*)"/test_define", test_define, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/test_resolve_global", test_resolve_global, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/test_resolve_local", test_resolve_local, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};
