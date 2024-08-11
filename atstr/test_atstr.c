#include "munit/munit.h"

#include "./atstr.h"

static MunitResult
test_split(__attribute__((unused)) const MunitParameter params[], __attribute__((unused)) void* user_data) {
  struct item {
    int offset;
    int length;
  };
  struct {
    const char *input;
    int count;
    struct item expected[16];
  } tests[] = {
    {
      "",
      0,
      {},
    },
    {
      "    ",
      0,
      {},
    },
    {
      "a",
      1,
      {
        { 0, 1 },
      }
    },
    {
      " a",
      1,
      {
        { 1, 1 },
      }
    },
    {
      "    a",
      1,
      {
        { 4, 1 },
      }
    },
    {
      "a ",
      1,
      {
        { 0, 1 },
      }
    },
    {
      "a   ",
      1,
      {
        { 0, 1 },
      }
    },
    {
      "abcdefg",
      1,
      {
        { 0, 7 },
      }
    },
    {
      "  abcdefg  ",
      1,
      {
        { 2, 7 },
      }
    },
    {
      "hello world",
      2,
      {
        { 0, 5 },
        { 6, 5 },
      }
    },
    {
      "hello  world",
      2,
      {
        { 0, 5 },
        { 7, 5 },
      }
    },
    {
      "hello world !  abcdefg \n ",
      4,
      {
        { 0, 5 },
        { 6, 5 },
        { 12, 1 },
        { 15, 7 },
      }
    },
    {
      "<view x=0  y=0 w=100 h=100>\n</view>",
      6,
      {
        {0, 5},
        {6, 3},
        {11, 3},
        {15, 5},
        {21, 6},
        {28, 7},
      }
    }
  };
  for (int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
    atstr_t atstr = atstr_new(tests[i].input);
    for (int j = 0; j < tests[i].count; j++) {
      atstr_t got = atstr_split_next(&atstr);
      munit_assert_ptr_equal(got.start, tests[i].input + tests[i].expected[j].offset);
      munit_assert_int(got.length, ==, tests[i].expected[j].length);
    }
    atstr_t got = atstr_split_next(&atstr);
    munit_assert_true(atstr_isnil(got));

    {
      atstr_t atstr = atstr_new(tests[i].input);
      ATSTR_SPLITN_WRAPPER(&atstr, tests[i].count, result, int ret);
      munit_assert_int(ret, ==, tests[i].count);
      for (int j = 0; j < tests[i].count; j++) {
        munit_assert_ptr_equal(result[j].start, tests[i].input + tests[i].expected[j].offset);
        munit_assert_int(result[j].length, ==, tests[i].expected[j].length);
      }
    }
  }
  return MUNIT_OK;
}

static MunitResult
test_splitc(__attribute__((unused)) const MunitParameter params[], __attribute__((unused)) void* user_data) {
    struct item {
    int offset;
    int length;
  };
  struct {
    const char *input;
    char c;
    int count;
    struct item expected[16];
  } tests[] = {
    {
      "a b",
      ' ',
      2,
      {
        { 0, 1 },
        { 2, 1 },
      },
    },
    {
      "x=100",
      '=',
      2,
      {
        { 0, 1 },
        { 2, 3 },
      }
    },
    {
      "x=100=y=200",
      '=',
      4,
      {
        { 0, 1 },
        { 2, 3 },
        { 6, 1 },
        {8, 3},
      },
    },
  };

  for (int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
    atstr_t atstr = atstr_new(tests[i].input);
    for (int j = 0; j < tests[i].count; j++) {
      atstr_t got = atstr_splitc_next(&atstr, tests[i].c);
      munit_assert_ptr_equal(got.start, tests[i].input + tests[i].expected[j].offset);
      munit_assert_int(got.length, ==, tests[i].expected[j].length);
    }
    atstr_t got = atstr_splitc_next(&atstr, tests[i].c);
    munit_assert_true(atstr_isnil(got));

    {
      atstr_t atstr = atstr_new(tests[i].input);
      ATSTR_SPLITCN_WRAPPER(&atstr, tests[i].c, tests[i].count, result, int ret);
      munit_assert_int(ret, ==, tests[i].count);
      for (int j = 0; j < tests[i].count; j++) {
        munit_assert_ptr_equal(result[j].start, tests[i].input + tests[i].expected[j].offset);
        munit_assert_int(result[j].length, ==, tests[i].expected[j].length);
      }
    }
  }

  return MUNIT_OK;
}

MunitTest atstr_tests[] = {
  { (char*) "/test_split", test_split, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
  { (char*) "/test_splitc", test_splitc, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
  { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
};

static const MunitSuite atstr_suite = {
  "/dai", /* name */
  atstr_tests, /* tests */
  NULL, /* suites */
  1, /* iterations */
  MUNIT_SUITE_OPTION_NONE /* options */
};

int
main(int argc, const char *argv[]) {
    return munit_suite_main(&atstr_suite, NULL, argc, (char *const *)argv);
}