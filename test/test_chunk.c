#include "munit/munit.h"

#include "dai_chunk.h"

static MunitResult
test_write(__attribute__((unused)) const MunitParameter params[], __attribute__((unused)) void* user_data) {
    struct {
        DaiOpCode op;
        uint16_t operand;
        int length;
        uint8_t expected[4];
    } tests[] = {
        {
            .op = DaiOpConstant,
            .operand = 65534,
            .length = 3,
            .expected = {DaiOpConstant, 255, 254},
        },
        {
            .op = DaiOpAdd,
            .operand = 0,
            .length = 1,
            .expected = {DaiOpAdd},
        }
    };
    for (int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        DaiChunk chunk;
        DaiChunk_init(&chunk, "<test_write>");
        switch (tests[i].length) {
            case 1:
                DaiChunk_write(&chunk, tests[i].op, 0);
                break;
            case 3:
                DaiChunk_writeu16(&chunk, tests[i].op, tests[i].operand, 0);
                break;
        }
        munit_assert_int(chunk.count, ==, tests[i].length);
        for (int j = 0; j < chunk.count; j++) {
            munit_assert_uint8(chunk.code[j], ==, tests[i].expected[j]);
        }
        DaiChunk_reset(&chunk);
    }
    return MUNIT_OK;
}

MunitTest chunk_tests[] = {
  { (char*) "/test_write", test_write, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
  { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
};
