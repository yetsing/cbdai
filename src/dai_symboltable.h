/*
符号表
*/
#ifndef FADFA587_97BF_4F17_B81D_02BA645443DF
#define FADFA587_97BF_4F17_B81D_02BA645443DF

#include <stdbool.h>

typedef enum {
    DaiSymbolType_builtin,   // 内置
    DaiSymbolType_global,    // 全局
    DaiSymbolType_local,     // 本地
    DaiSymbolType_free,      // 自由
    DaiSymbolType_self,      // self
} DaiSymbolType;

typedef struct {
    char* name;
    int depth;   // -1 表示内置，0 表示全局，> 0 都是局部
    int index;
    bool defined;   // 是否已经定义，用于全局变量，全局变量会先预定义一波，以备后续解析
    DaiSymbolType type;
    bool is_const;   // 是否是 const variable (const variable 不能被重新赋值)
} DaiSymbol;

typedef struct _DaiSymbolTable DaiSymbolTable;

DaiSymbolTable*
DaiSymbolTable_New();
DaiSymbolTable*
DaiSymbolTable_NewEnclosed(DaiSymbolTable* outer);
DaiSymbolTable*
DaiSymbolTable_NewFunction(DaiSymbolTable* outer);
void
DaiSymbolTable_free(DaiSymbolTable* table);
void
DaiSymbolTable_setOuter(DaiSymbolTable* table, DaiSymbolTable* outer);
DaiSymbol
DaiSymbolTable_predefine(DaiSymbolTable* table, const char* name);
DaiSymbol
DaiSymbolTable_define(DaiSymbolTable* table, const char* name, bool is_const);
DaiSymbol
DaiSymbolTable_defineBuiltin(DaiSymbolTable* table, int index, const char* name);
DaiSymbol
DaiSymbolTable_defineSelf(DaiSymbolTable* table);
// 如果找到返回 true，否则返回 false
bool
DaiSymbolTable_resolve(DaiSymbolTable* table, const char* name, DaiSymbol* symbol);
bool
DaiSymbolTable_isDefined(DaiSymbolTable* table, const char* name);
int
DaiSymbolTable_count(DaiSymbolTable* table);
int
DaiSymbolTable_countOuter(DaiSymbolTable* table);
DaiSymbol*
DaiSymbolTable_getFreeSymbols(DaiSymbolTable* table, int* free_symbol_count);
bool
DaiSymbolTable_isLocal(DaiSymbolTable* table);

#endif /* FADFA587_97BF_4F17_B81D_02BA645443DF */
