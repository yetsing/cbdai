#include <stdlib.h>
#include <string.h>

#include "dai_memory.h"
#include "dai_object.h"
#include "dai_table.h"
#include "dai_value.h"

#define TABLE_MAX_LOAD 0.75

void
DaiTable_init(DaiTable* table) {
    table->count    = 0;
    table->capacity = 0;
    table->entries  = NULL;
}
void
DaiTable_reset(DaiTable* table) {
    FREE_ARRAY(Entry, table->entries, table->capacity);
    DaiTable_init(table);
}
// NOTE: The "Optimization" chapter has a manual copy of this function.
// If you change it here, make sure to update that copy.
static Entry*
find_entry(Entry* entries, int capacity, DaiObjString* key) {
    uint32_t index     = key->hash % capacity;
    Entry*   tombstone = NULL;

    for (;;) {
        Entry* entry = &entries[index];
        if (entry->key == NULL) {
            if (IS_NIL(entry->value)) {
                // Empty entry.
                return tombstone != NULL ? tombstone : entry;
            } else {
                // We found a tombstone.
                if (tombstone == NULL) tombstone = entry;
            }
        } else if (entry->key == key) {
            // We found the key.
            return entry;
        }

        index = (index + 1) % capacity;
    }
}
bool
DaiTable_has(DaiTable* table, DaiObjString* key) {
    if (table->count == 0) {
        return false;
    }
    Entry* entry = find_entry(table->entries, table->capacity, key);
    return entry->key != NULL;
}

bool
DaiTable_get(DaiTable* table, DaiObjString* key, DaiValue* value) {
    if (table->count == 0) return false;

    Entry* entry = find_entry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    *value = entry->value;
    return true;
}
static void
adjust_capacity(DaiTable* table, int capacity) {
    Entry* entries = ALLOCATE(Entry, capacity);
    for (int i = 0; i < capacity; i++) {
        entries[i].key   = NULL;
        entries[i].value = NIL_VAL;
    }

    table->count = 0;
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key == NULL) continue;

        Entry* dest = find_entry(entries, capacity, entry->key);
        dest->key   = entry->key;
        dest->value = entry->value;
        table->count++;
    }

    FREE_ARRAY(Entry, table->entries, table->capacity);
    table->entries  = entries;
    table->capacity = capacity;
}
bool
DaiTable_set(DaiTable* table, DaiObjString* key, DaiValue value) {
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        int capacity = GROW_CAPACITY(table->capacity);
        adjust_capacity(table, capacity);
    }

    Entry* entry    = find_entry(table->entries, table->capacity, key);
    bool   isNewKey = entry->key == NULL;
    if (isNewKey && IS_NIL(entry->value)) table->count++;

    entry->key   = key;
    entry->value = value;
    return isNewKey;
}

bool
DaiTable_set_if_exist(DaiTable* table, DaiObjString* key, DaiValue value) {
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        int capacity = GROW_CAPACITY(table->capacity);
        adjust_capacity(table, capacity);
    }

    Entry* entry = find_entry(table->entries, table->capacity, key);
    if (entry->key == NULL) {
        return false;
    }

    entry->key   = key;
    entry->value = value;
    return true;
}

bool
DaiTable_delete(DaiTable* table, DaiObjString* key) {
    if (table->count == 0) return false;

    // Find the entry.
    Entry* entry = find_entry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    // Place a tombstone in the entry.
    entry->key   = NULL;
    entry->value = BOOL_VAL(true);
    return true;
}
void
DaiTable_addAll(DaiTable* from, DaiTable* to) {
    for (int i = 0; i < from->capacity; i++) {
        Entry* entry = &from->entries[i];
        if (entry->key != NULL) {
            DaiTable_set(to, entry->key, entry->value);
        }
    }
}
void
DaiTable_copy(DaiTable* from, DaiTable* to) {
    memcpy(to, from, sizeof(DaiTable));
    to->entries = ALLOCATE(Entry, from->capacity);
    memcpy(to->entries, from->entries, sizeof(Entry) * from->capacity);
}

DaiObjString*
DaiTable_findString(DaiTable* table, const char* chars, int length, uint32_t hash) {
    if (table->count == 0) return NULL;

    uint32_t index = hash % table->capacity;
    for (;;) {
        Entry* entry = &table->entries[index];
        if (entry->key == NULL) {
            // Stop if we find an empty non-tombstone entry.
            if (IS_NIL(entry->value)) return NULL;
        } else if (entry->key->length == length && entry->key->hash == hash &&
                   memcmp(entry->key->chars, chars, length) == 0) {
            // We found it.
            return entry->key;
        }

        index = (index + 1) % table->capacity;
    }
}

void
tableRemoveWhite(DaiTable* table) {
    for (int i = 0; i < table->capacity; ++i) {
        Entry* entry = &table->entries[i];
        if (entry->key != NULL && !entry->key->obj.is_marked) {
            DaiTable_delete(table, entry->key);
        }
    }
}
