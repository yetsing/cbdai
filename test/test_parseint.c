#include <stdint.h>
#include <stdio.h>

#include "munit/munit.h"

#include "dai_parseint.h"

static MunitResult
test_parseint(__attribute__((unused)) const MunitParameter params[],
              __attribute__((unused)) void* user_data) {
    struct TestCase {
        const char* str;
        int base;
        int64_t expected;
        const char* error;
    } test_cases[] = {
        {"0", 10, 0, NULL},
        {"1", 10, 1, NULL},
        {"-1", 10, -1, NULL},
        {"+1", 10, 1, NULL},
        {"1", 10, 1, NULL},
        {"+1", 10, 1, NULL},
        {"-1", 10, -1, NULL},
        {"4294967296", 10, 4294967296LL, NULL},
        {"9223372036854775807", 10, 9223372036854775807LL, NULL},
        {"+9223372036854775807", 10, 9223372036854775807LL, NULL},
        {"-9223372036854775807", 10, -9223372036854775807LL, NULL},
        // ref: https://github.com/emscripten-core/emscripten/issues/10336#issuecomment-581166814
        // 为了避免 warning: integer literal is too large to be represented in a signed integer
        // type, interpreting as unsigned [-Wimplicitly-unsigned-literal]
        // 这里减一，而不是直接写最终数值
        {"-9223372036854775808", 10, -9223372036854775807LL - 1, NULL},

        {"0b1", 2, 1, NULL},
        {"+0b1", 2, 1, NULL},
        {"-0b1", 2, -1, NULL},
        {"-0B10", 2, -2, NULL},
        {"-10", 2, -2, NULL},
        {"0b100000000000000000000000000000000", 2, 4294967296, NULL},
        {"0b111111111111111111111111111111111111111111111111111111111111111",
         2,
         9223372036854775807LL,
         NULL},
        {"-0b111111111111111111111111111111111111111111111111111111111111111",
         2,
         -9223372036854775807LL,
         NULL},
        {"-0b1000000000000000000000000000000000000000000000000000000000000000",
         2,
         -9223372036854775807LL - 1,
         NULL},

        {"0o1", 8, 1, NULL},
        {"+0o1", 8, 1, NULL},
        {"-0o1", 8, -1, NULL},
        {"-0O10", 8, -8, NULL},
        {"-0O_10", 8, -8, NULL},
        {"-10", 8, -8, NULL},
        {"0o777777777777777777777", 8, 9223372036854775807LL, NULL},
        {"-0o777777777777777777777", 8, -9223372036854775807LL, NULL},
        {"-0o1000000000000000000000", 8, -9223372036854775807LL - 1, NULL},

        {"0x1", 16, 1, NULL},
        {"+0x1", 16, 1, NULL},
        {"-0x1", 16, -1, NULL},
        {"-0X10", 16, -16, NULL},
        {"-10", 16, -16, NULL},
        {"a", 16, 10, NULL},
        {"0x0123456789abcdef", 16, 81985529216486895, NULL},
        {"0X0123456789ABCDEF", 16, 81985529216486895, NULL},
        {"0x7fffffffffffffff", 16, 9223372036854775807LL, NULL},
        {"-0x7fffffffffffffff", 16, -9223372036854775807LL, NULL},
        {"-0x8000000000000000", 16, -9223372036854775807LL - 1, NULL},

        {"1234567890", 36, 107446288850820LL, NULL},
        {"abcdefghij", 36, 1047601316295595LL, NULL},
        {"ABCDEFGHIJ", 36, 1047601316295595LL, NULL},
        {"hijklmnopq", 36, 1778833004308190LL, NULL},
        {"HIJKLMNOPQ", 36, 1778833004308190LL, NULL},
        {"rstuvwxyz", 36, 78429158374139LL, NULL},
        {"RSTUVWXYZ", 36, 78429158374139LL, NULL},
        {"rsTUVWxyz", 36, 78429158374139LL, NULL},
        {"r_s_T_U_VWxyz", 36, 78429158374139LL, NULL},

        {"", 16, 0, "empty string"},
        {"1", 0, 0, "invalid base"},
        {"1", 1, 0, "invalid base"},
        {"1", 37, 0, "invalid base"},
        {"0b123", 16, 0, "invalid base prefix"},
        {"0x123", 2, 0, "invalid base prefix"},
        {"0x123", 8, 0, "invalid base prefix"},
        {"0x123", 10, 0, "invalid base prefix"},
        {"0123", 10, 0, "invalid base prefix"},
        {"12a3", 10, 0, "invalid character in number"},
        {"a", 10, 0, "invalid character in number"},
        {"12#3", 10, 0, "invalid character in number"},
        {"#", 10, 0, "invalid character in number"},
        {"3w5e11264sgsz", 36, 0, "integer overflow"},
        {"3w5e11264sgtz", 36, 0, "integer overflow"},
        {"9223372036854775808", 10, 0, "integer overflow"},
        {"19223372036854775808", 10, 0, "integer overflow"},
        {"_123", 10, 0, "invalid underscores usage in number"},
        {"123_", 10, 0, "invalid underscores usage in number"},
        {"1__23", 10, 0, "invalid underscores usage in number"},
    };

    char* error;
    for (int i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
        int64_t n = dai_parseint(test_cases[i].str, test_cases[i].base, &error);
        if (test_cases[i].error != NULL) {
            munit_assert_ptr_not_null(error);
            munit_assert_string_equal(error, test_cases[i].error);
            continue;
        }
        munit_assert_null(error);
        munit_assert_int64(n, ==, test_cases[i].expected);
        error = NULL;
    }
    return MUNIT_OK;
}

MunitTest parseint_tests[] = {
    {(char*)"/test_parseint", test_parseint, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};
