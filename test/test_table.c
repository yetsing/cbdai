#include "munit/munit.h"

#include "dai_malloc.h"
#include "dai_object.h"
#include "dai_table.h"

static uint32_t
hash_string(const char* key, int length) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}

static DaiObjString*
new_obj_string(char* s) {
    DaiObjString* obj = dai_malloc(sizeof(DaiObjString));
    obj->obj.type     = DaiObjType_string;
    obj->obj.next     = NULL;
    obj->length       = strlen(s);
    obj->chars        = strdup(s);
    obj->hash         = hash_string(s, obj->length);
    return obj;
}

static void
free_obj_string(DaiObjString* obj) {
    dai_free(obj->chars);
    dai_free(obj);
}

static MunitResult
test_crud(__attribute__((unused)) const MunitParameter params[],
          __attribute__((unused)) void* user_data) {
    DaiTable table;
    DaiTable_init(&table);
    DaiObjString* keys[] = {
        new_obj_string("a"),
        new_obj_string("b"),
        new_obj_string("c"),
        new_obj_string("d"),
        new_obj_string("e"),
        new_obj_string("f"),
        new_obj_string("g"),
        new_obj_string("hello"),
        new_obj_string("world"),
    };
    size_t count = sizeof(keys) / sizeof(DaiObjString*);
    for (int i = 0; i < count; i++) {
        DaiObjString* key = keys[i];
        bool newkey       = DaiTable_set(&table, key, INTEGER_VAL(i));
        munit_assert_true(newkey);
        DaiValue got;
        bool found = DaiTable_get(&table, key, &got);
        munit_assert_true(found);
        munit_assert_true(dai_value_equal(got, INTEGER_VAL(i)) == 1);
    }
    for (int i = 0; i < count; i++) {
        DaiObjString* key = keys[i];
        bool newkey       = DaiTable_set(&table, key, INTEGER_VAL(i));
        munit_assert_false(newkey);
        DaiValue got;
        bool found = DaiTable_get(&table, key, &got);
        munit_assert_true(found);
        munit_assert_true(dai_value_equal(got, INTEGER_VAL(i)) == 1);
    }

    for (int i = 0; i < count; i++) {
        bool deleted = DaiTable_delete(&table, keys[i]);
        munit_assert_true(deleted);
        DaiValue got;
        bool found = DaiTable_get(&table, keys[i], &got);
        munit_assert_false(found);
    }
    for (int i = 0; i < count; i++) {
        DaiObjString* key = keys[i];
        bool newkey       = DaiTable_set(&table, key, INTEGER_VAL(i));
        munit_assert_true(newkey);
        DaiValue got;
        bool found = DaiTable_get(&table, key, &got);
        munit_assert_true(found);
        munit_assert_true(dai_value_equal(got, INTEGER_VAL(i)) == 1);
    }

    for (int i = 0; i < count; i++) {
        free_obj_string(keys[i]);
    }
    DaiTable_reset(&table);
    return MUNIT_OK;
}

MunitTest table_tests[] = {
    {(char*)"/test_crud", test_crud, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};
