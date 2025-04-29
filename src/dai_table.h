#ifndef CBDAI_DAI_TABLE_H
#define CBDAI_DAI_TABLE_H

#include "dai_value.h"

typedef struct {
    DaiObjString* key;
    DaiValue value;
} Entry;

typedef struct {
    int count;
    int capacity;
    Entry* entries;
} DaiTable;

void
DaiTable_init(DaiTable* table);
void
DaiTable_reset(DaiTable* table);
bool
DaiTable_has(DaiTable* table, DaiObjString* key);
bool
DaiTable_get(DaiTable* table, DaiObjString* key, DaiValue* value);
bool
DaiTable_set(DaiTable* table, DaiObjString* key, DaiValue value);
// key存在才更新值, 否则返回false
bool
DaiTable_set_if_exist(DaiTable* table, DaiObjString* key, DaiValue value);
bool
DaiTable_delete(DaiTable* table, DaiObjString* key);
void
DaiTable_addAll(DaiTable* from, DaiTable* to);
void
DaiTable_copy(DaiTable* from, DaiTable* to);
DaiObjString*
DaiTable_findString(DaiTable* table, const char* chars, int length, uint32_t hash);
void
tableRemoveWhite(DaiTable* table);
#endif /* CBDAI_DAI_TABLE_H */
