#include <stdio.h>

#include "munit/munit.h"

#include "dai_debug.h"
#include "dai_object.h"
#include "dai_value.h"

void
dai_assert_value_equal(DaiValue actual, DaiValue expected) {
    if (IS_OBJ(expected)) {
        if (!IS_OBJ(actual)) {
            fprintf(stderr, "expected an object, but got %s\n", dai_value_ts(actual));
            munit_assert_true(IS_OBJ(actual));
        }
        if (IS_STRING(expected)) {
            if (!IS_STRING(actual)) {
                fprintf(stderr, "expected a string, but got %s\n", dai_value_ts(actual));
                munit_assert_true(IS_STRING(actual));
            }
            munit_assert_string_equal(AS_CSTRING(actual), AS_CSTRING(expected));
        } else if (IS_FUNCTION(expected)) {
            if (!IS_FUNCTION(actual)) {
                fprintf(stderr, "expected a function, but got %s\n", dai_value_ts(actual));
                munit_assert_true(IS_FUNCTION(actual));
            }
            const char* expected_name = DaiObjFunction_name(AS_FUNCTION(expected));
            const char* actual_name   = DaiObjFunction_name(AS_FUNCTION(actual));
            if (expected_name[0] != '<') {
                munit_assert_string_equal(expected_name, actual_name);
            }
            DaiChunk* expected_chunk = &AS_FUNCTION(expected)->chunk;
            DaiChunk* actual_chunk   = &AS_FUNCTION(actual)->chunk;
            if (expected_chunk->count != actual_chunk->count) {
                DaiChunk_disassemble(actual_chunk, "actual");
                printf("expected function: %s\n", DaiObjFunction_name(AS_FUNCTION(expected)));
            }
            munit_assert_int(expected_chunk->count, ==, actual_chunk->count);
            for (int i = 0; i < expected_chunk->count; i++) {
                if (expected_chunk->code[i] != actual_chunk->code[i]) {
                    DaiChunk_disassemble(actual_chunk, "actual");
                    printf("i=%d\n", i);
                    printf("expected function: %s\n", DaiObjFunction_name(AS_FUNCTION(expected)));
                }
                munit_assert_int(expected_chunk->code[i], ==, actual_chunk->code[i]);
            }
        } else if (IS_CLOSURE(expected)) {
            if (!IS_CLOSURE(actual)) {
                fprintf(stderr, "expected a closure, but got %d\n", OBJ_TYPE(actual));
                munit_assert_true(IS_CLOSURE(actual));
            }
            dai_assert_value_equal(OBJ_VAL(AS_CLOSURE(actual)->function),
                                   OBJ_VAL(AS_CLOSURE(expected)->function));
        } else if (IS_ERROR(expected)) {
            if (!IS_ERROR(actual)) {
                fprintf(stderr, "expected a error, but got %d\n", OBJ_TYPE(actual));
                munit_assert_true(IS_ERROR(actual));
            }
            munit_assert_string_equal(AS_ERROR(actual)->message, AS_ERROR(expected)->message);
        } else {
            fprintf(stderr, "not support constant type %d\n", OBJ_TYPE(expected));
            munit_assert_true(false);
        }
    } else if (dai_value_equal(actual, expected) != 1) {
        printf("actual=(%s)", dai_value_ts(actual));
        dai_print_value(actual);
        printf(" expected=(%s)", dai_value_ts(expected));
        dai_print_value(expected);
        printf("\n");
        munit_assert_true(false);
    }
}