#ifndef EC3F9B25_451A_484C_906A_F9BFCA85A45A
#define EC3F9B25_451A_484C_906A_F9BFCA85A45A

#include "dai_common.h"
#include "dai_value.h"

typedef struct {
    DaiObjString* key;
    DaiValue      value;
} Entry;

typedef struct {
    int    count;
    int    capacity;
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
#endif /* EC3F9B25_451A_484C_906A_F9BFCA85A45A */
