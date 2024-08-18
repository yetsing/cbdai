#include <stdio.h>

#include "dai_stringbuffer.h"
#include "munit/munit.h"

static MunitResult
test_stringbuffer(__attribute__((unused)) const MunitParameter params[],
                  __attribute__((unused)) void* user_data) {
    DaiStringBuffer* sb = DaiStringBuffer_New();
    munit_assert_int(DaiStringBuffer_length(sb), ==, 0);

    DaiStringBuffer_write(sb, "hello");
    munit_assert_int(DaiStringBuffer_length(sb), ==, 5);

    DaiStringBuffer_writen(sb, "world\n", 5);
    munit_assert_int(DaiStringBuffer_length(sb), ==, 10);

    char* s = DaiStringBuffer_getAndFree(sb, NULL);
    munit_assert_string_equal(s, "helloworld");
    free(s);

    sb = DaiStringBuffer_New();
    munit_assert_int(DaiStringBuffer_length(sb), ==, 0);
    int n = 10;
    for (int i = 0; i < n; i++) {
        DaiStringBuffer_writeInt(sb, 12345);
    }
    munit_assert_int(DaiStringBuffer_length(sb), ==, 5 * n);
    s = DaiStringBuffer_getAndFree(sb, NULL);
    munit_assert_string_equal(s, "12345123451234512345123451234512345123451234512345");
    free(s);

    sb = DaiStringBuffer_New();
    munit_assert_int(DaiStringBuffer_length(sb), ==, 0);
    DaiStringBuffer_writePointer(sb, (void*)0x3039);
    munit_assert_int(DaiStringBuffer_length(sb), ==, 6);
    s = DaiStringBuffer_getAndFree(sb, NULL);
    munit_assert_string_equal(s, "0x3039");
    free(s);

    sb = DaiStringBuffer_New();
    munit_assert_int(DaiStringBuffer_length(sb), ==, 0);
    DaiStringBuffer_writeInt64(sb, 12345);
    munit_assert_int(DaiStringBuffer_length(sb), ==, 5);
    DaiStringBuffer_writef(sb, "%d", 12345);
    munit_assert_int(DaiStringBuffer_length(sb), ==, 10);
    s = DaiStringBuffer_getAndFree(sb, NULL);
    munit_assert_string_equal(s, "1234512345");
    free(s);
    return MUNIT_OK;
}

static MunitResult
test_stringbuffer_line_prefix(__attribute__((unused)) const MunitParameter params[],
                              __attribute__((unused)) void* user_data) {
    char buf[1024];
    size_t length;
    DaiStringBuffer* sb = DaiStringBuffer_New();
    munit_assert_int(DaiStringBuffer_length(sb), ==, 0);
    DaiStringBuffer_writeWithLinePrefix(sb, "hello", ">>");
    munit_assert_int(DaiStringBuffer_length(sb), ==, 7);
    char* s = DaiStringBuffer_getAndFree(sb, &length);
    munit_assert_size(length, ==, 7);
    munit_assert_string_equal(s, ">>hello");
    free(s);

    sb = DaiStringBuffer_New();
    munit_assert_int(DaiStringBuffer_length(sb), ==, 0);
    snprintf(buf, 1024, "hello\n");
    DaiStringBuffer_writeWithLinePrefix(sb, buf, ">>");
    munit_assert_int(DaiStringBuffer_length(sb), ==, 8);
    snprintf(buf, 1024, "hello\n");
    DaiStringBuffer_writeWithLinePrefix(sb, buf, ">>");
    munit_assert_int(DaiStringBuffer_length(sb), ==, 16);
    s = DaiStringBuffer_getAndFree(sb, &length);
    munit_assert_size(length, ==, 16);
    munit_assert_string_equal(s, ">>hello\n>>hello\n");
    free(s);

    sb = DaiStringBuffer_New();
    munit_assert_int(DaiStringBuffer_length(sb), ==, 0);
    snprintf(buf, 1024, "hello\nworld");
    DaiStringBuffer_writeWithLinePrefix(sb, buf, ">>");
    munit_assert_int(DaiStringBuffer_length(sb), ==, 15);
    s = DaiStringBuffer_getAndFree(sb, &length);
    munit_assert_size(length, ==, 15);
    munit_assert_string_equal(s, ">>hello\n>>world");
    free(s);

    return MUNIT_OK;
}

MunitTest stringbuffer_tests[] = {
    {(char*)"/test_stringbuffer", test_stringbuffer, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/test_stringbuffer_line_prefix",
     test_stringbuffer_line_prefix,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};
