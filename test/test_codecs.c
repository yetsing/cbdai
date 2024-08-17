#include "munit/munit.h"

#include "dai_codecs.h"

static MunitResult
test_utf8_decode(__attribute__((unused)) const MunitParameter params[],
                 __attribute__((unused)) void*                user_data) {
    struct {
        const char* input;
        dai_rune_t  expected;
        int         expected_len;
    } tests[] = {
        {"a", 97, 1},
        {"α", 945, 2},
        {"中", 20013, 3},
        {"😁", 128513, 4},
        {"\xf0\x9f\x98\x81", 128513, 4},
        {"\xf8\x88\x80\x80\x80", 0x200000, 5},
        {"\xf8\x88\x80\x80\x81", 0x200001, 5},
        {"\xfc\x84\x80\x80\x80\x80", 0x4000000, 6},
        {"\xfc\x84\x80\x80\x80\x81", 0x4000001, 6},
        {"\xfd\xbf\xbf\xbf\xbf\xbf", 0x7FFFFFFF, 6},

        // 错误的UTF-8编码
        {"\xbf", 0, -1},
        {"\x80", 0, -1},
        {"\xc0\xc0", 0, -1},
        {"\xf0\x9f\x98\xc1", 0, -1},
        {"\xf1\x80\x80\xc0", 0, -1},
        {"\xf9\x81\x80\x80\xc0", 0, -1},
        {"\xfd\x88\x81\x80\x80\xc0", 0, -1},
        // 编码格式正确，但是数值不在编码范围内
        // 比如 2 字节的 utf8 编码要求 [0x0080, 0x07ff]
        {"\xc1\xa1", 0, -1},
        {"\xe0\x81\xa1", 0, -1},
        {"\xf0\x80\x81\xa1", 0, -1},
        {"\xf8\x80\x80\x81\xa1", 0, -1},
        {"\xfc\x80\x80\x80\x81\xa1", 0, -1},
    };
    for (int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        dai_rune_t rune = 0;
        int        len  = dai_utf8_decode(tests[i].input, &rune);
        munit_assert_int(len, ==, tests[i].expected_len);
        if (len != -1) {
            munit_assert_uint32(rune, ==, tests[i].expected);
        }
    }
    return MUNIT_OK;
}

MunitTest codecs_tests[] = {
    {(char*)"/test_utf8_decode", test_utf8_decode, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};
