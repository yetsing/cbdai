/*
符号表
*/

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "dai_common.h"
#include "dai_memory.h"
#include "dai_symboltable.h"

// #region SymbolMap 结构体及其方法
#define TABLE_MAX_LOAD 0.75
typedef struct {
    char*     key;
    int       key_length;
    DaiSymbol value;
} SymbolEntry;

typedef struct {
    int          count;
    int          capacity;
    SymbolEntry* entries;
} SymbolMap;

void
SymbolMap_init(SymbolMap* table) {
    table->count    = 0;
    table->capacity = 0;
    table->entries  = NULL;
}

void
SymbolMap_reset(SymbolMap* table) {
    for (int i = 0; i < table->capacity; i++) {
        SymbolEntry* entry = &table->entries[i];
        if (entry->key != NULL) {
            assert(entry->key == entry->value.name);
            FREE_ARRAY(char, entry->key, entry->key_length + 1);
            entry->key        = NULL;
            entry->value.name = NULL;
        }
    }
    FREE_ARRAY(SymbolEntry, table->entries, table->capacity);
    SymbolMap_init(table);
}

static uint32_t
hash_string(const char* key, int length) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}

static SymbolEntry*
find_entry(SymbolEntry* entries, int capacity, const char* key, int length) {
    uint32_t hash  = hash_string(key, length);
    uint32_t index = hash % capacity;

    for (;;) {
        SymbolEntry* entry = &entries[index];
        if (entry->key == NULL) {
            return entry;
        } else if (length == entry->key_length && strcmp(entry->key, key) == 0) {
            return entry;
        }
        index = (index + 1) % capacity;
    }
}

bool
SymbolMap_get(SymbolMap* table, const char* key, DaiSymbol* value) {
    if (table->count == 0) return false;


    SymbolEntry* entry = find_entry(table->entries, table->capacity, key, strlen(key));
    if (entry->key == NULL) return false;

    *value = entry->value;
    return true;
}

static void
adjust_capacity(SymbolMap* table, int capacity) {
    SymbolEntry* entries = ALLOCATE(SymbolEntry, capacity);
    for (int i = 0; i < capacity; i++) {
        entries[i].key   = NULL;
        entries[i].value = (DaiSymbol){NULL, 0, 0};
    }

    table->count = 0;
    for (int i = 0; i < table->capacity; i++) {
        SymbolEntry* entry = &table->entries[i];
        if (entry->key == NULL) continue;

        SymbolEntry* dest = find_entry(entries, capacity, entry->key, entry->key_length);
        dest->key         = entry->key;
        dest->key_length  = entry->key_length;
        dest->value       = entry->value;
        table->count++;
    }

    FREE_ARRAY(SymbolEntry, table->entries, table->capacity);
    table->entries  = entries;
    table->capacity = capacity;
}

bool
SymbolMap_set(SymbolMap* table, char* key, DaiSymbol value) {
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        int capacity = GROW_CAPACITY(table->capacity);
        adjust_capacity(table, capacity);
    }

    int          key_length = strlen(key);
    SymbolEntry* entry      = find_entry(table->entries, table->capacity, key, key_length);
    bool         isNewKey   = entry->key == NULL;
    if (isNewKey) {
        table->count++;
    }

    entry->key        = key;
    entry->key_length = key_length;
    entry->value      = value;
    return isNewKey;
}

typedef struct {
    int          offset;
    int          capacity;
    SymbolEntry* entries;
} SymbolMapIterator;

__attribute__((unused)) static DaiSymbol
SymbolMapIterator_next(SymbolMapIterator* iterator) {
    if (iterator->offset >= iterator->capacity) {
        return (DaiSymbol){NULL, 0, 0};
    }
    for (int i = iterator->offset + 1; i < iterator->capacity; i++) {
        SymbolEntry* entry = &iterator->entries[i];
        if (entry->key == NULL) {
            continue;
        }
        iterator->offset = i;
        return entry->value;
    }
    return (DaiSymbol){NULL, 0, 0};
}

__attribute__((unused)) static SymbolMapIterator
SymbolMap_newIterator(SymbolMap* table) {
    return (SymbolMapIterator){0, table->capacity, table->entries};
}

// #endregion

typedef struct _DaiSymbolTable {
    SymbolMap       store;
    int             num_symbols;
    int             num_symbols_of_outer;
    int             depth;   // 0 表示全局，> 0 都是局部
    DaiSymbolTable* outer;
    int             function_depth;   // 函数所在层级

    int       num_free;   // 自由变量数
    DaiSymbol free_symbols[256];
} DaiSymbolTable;

static bool
DaiSymbolTable_isLocal(DaiSymbolTable* table) {
    return table->depth > 0;
}

DaiSymbolTable*
DaiSymbolTable_New() {
    DaiSymbolTable* table = ALLOCATE(DaiSymbolTable, 1);
    SymbolMap_init(&table->store);
    table->num_symbols          = 0;
    table->num_symbols_of_outer = 0;
    table->depth                = 0;
    table->outer                = NULL;
    table->function_depth       = 0;
    table->num_free             = 0;
    return table;
}
DaiSymbolTable*
DaiSymbolTable_NewEnclosed(DaiSymbolTable* outer) {
    DaiSymbolTable* table = DaiSymbolTable_New();
    table->depth          = outer->depth + 1;
    table->outer          = outer;
    if (DaiSymbolTable_isLocal(outer)) {
        table->num_symbols_of_outer = outer->num_symbols_of_outer + outer->num_symbols;
    }
    table->function_depth = outer->function_depth;
    return table;
}

DaiSymbolTable*
DaiSymbolTable_NewFunction(DaiSymbolTable* outer) {
    DaiSymbolTable* table       = DaiSymbolTable_New();
    table->depth                = outer->depth + 1;
    table->outer                = outer;
    table->num_symbols_of_outer = 0;
    table->function_depth       = outer->function_depth + 1;
    return table;
}

void
DaiSymbolTable_free(DaiSymbolTable* table) {
    SymbolMap_reset(&table->store);
    FREE(DaiSymbolTable, table);
}

DaiSymbol
DaiSymbolTable_predefine(DaiSymbolTable* table, const char* name) {
    char*     cname  = strdup(name);
    DaiSymbol symbol = {
        cname, table->depth, table->num_symbols + table->num_symbols_of_outer, false};
    if (table->depth == 0) {
        symbol.type = DaiSymbolType_global;
    } else {
        symbol.type = DaiSymbolType_local;
    }
    table->num_symbols++;
    bool isNewKey = SymbolMap_set(&table->store, symbol.name, symbol);
    assert(isNewKey);
    return symbol;
}

DaiSymbol
DaiSymbolTable_define(DaiSymbolTable* table, const char* name) {
    DaiSymbol symbol;
    bool      found = SymbolMap_get(&table->store, name, &symbol);
    if (found) {
        assert(!symbol.defined);
        symbol.defined = true;
    } else {
        char* cname = strdup(name);
        int   index = table->num_symbols + table->num_symbols_of_outer;
        symbol      = (DaiSymbol){cname, table->depth, index, true};
    }
    if (table->depth == 0) {
        symbol.type = DaiSymbolType_global;
    } else {
        symbol.type = DaiSymbolType_local;
    }
    table->num_symbols++;
    bool isNewKey = SymbolMap_set(&table->store, symbol.name, symbol);
    assert((found && !isNewKey) || (!found && isNewKey));
    return symbol;
}

DaiSymbol
DaiSymbolTable_defineBuiltin(DaiSymbolTable* table, int index, const char* name) {
    char*     cname    = strdup(name);
    DaiSymbol symbol   = (DaiSymbol){cname, -1, index, true, DaiSymbolType_builtin};
    bool      isNewKey = SymbolMap_set(&table->store, symbol.name, symbol);
    assert(isNewKey);
    return symbol;
}

DaiSymbol
DaiSymbolTable_defineSelf(DaiSymbolTable* table) {
    assert(table->num_symbols == 0 && table->num_symbols_of_outer == 0);
    char*     cname    = strdup("self");
    DaiSymbol symbol   = (DaiSymbol){cname, table->depth, 0, true, DaiSymbolType_self};
    bool      isNewKey = SymbolMap_set(&table->store, symbol.name, symbol);
    assert(isNewKey);
    table->num_symbols++;
    return symbol;
}

DaiSymbol
DaiSymbolTable_defineFree(DaiSymbolTable* table, DaiSymbol original) {
    table->free_symbols[table->num_free] = original;
    char*     cname                      = strdup(original.name);
    DaiSymbol symbol =
        (DaiSymbol){cname, original.depth, table->num_free, true, DaiSymbolType_free};
    bool isNewKey = SymbolMap_set(&table->store, symbol.name, symbol);
    table->num_free++;
    assert(isNewKey);
    return symbol;
}

// 如果找到返回 true，否则返回 false
bool
DaiSymbolTable_resolve(DaiSymbolTable* table, const char* name, DaiSymbol* symbol) {
    bool found = SymbolMap_get(&table->store, name, symbol);
    if (!found && table->outer != NULL) {
        found = DaiSymbolTable_resolve(table->outer, name, symbol);
        if (found &&
            (symbol->type != DaiSymbolType_builtin && symbol->type != DaiSymbolType_global) &&
            table->function_depth > table->outer->function_depth) {
            *symbol = DaiSymbolTable_defineFree(table, *symbol);
        }
    }
    return found;
}

bool
DaiSymbolTable_isDefined(DaiSymbolTable* table, const char* name) {
    DaiSymbol symbol;
    return SymbolMap_get(&table->store, name, &symbol) && symbol.defined;
}

int
DaiSymbolTable_count(DaiSymbolTable* table) {
    return table->num_symbols;
}

DaiSymbol*
DaiSymbolTable_getFreeSymbols(DaiSymbolTable* table, int* free_symbol_count) {
    *free_symbol_count = table->num_free;
    return table->free_symbols;
}
