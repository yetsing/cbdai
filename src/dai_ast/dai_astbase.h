#ifndef CBDAI_DAI_ASTBASE_H
#define CBDAI_DAI_ASTBASE_H

#include <stdbool.h>

#include "dai_ast/dai_asttype.h"
#include "dai_tokenize.h"

// #region DaiAst 基类
typedef struct _DaiAstBase DaiAstBase;
// 每个 ast 节点都有下面这两个函数
// 返回 ast 节点的字符串，方便查看和调试
// 字符串是分配在堆上的，需要调用者自己释放
typedef char* (*DaiAstStringFn)(DaiAstBase* ast, bool recursive);
// 释放 ast 节点
typedef void (*DaiAstFreeFn)(DaiAstBase* ast, bool recursive);

// ast 节点公共部分，每个 ast 结构体的前面都要有这个部分
// 包含的 token 为 [start_token, ..., end_token) （start_token end_token 仅为引用）
// 行列位置为 [(start_line, start_column), ..., (end_line, end_column))
#define DAI_AST_BASE_HEAD     \
    DaiAstType type;          \
    DaiAstStringFn string_fn; \
    DaiAstFreeFn free_fn;     \
    DaiToken* start_token;    \
    DaiToken* end_token;      \
    int start_line;           \
    int start_column;         \
    int end_line;             \
    int end_column;

typedef struct _DaiAstBase {
    DAI_AST_BASE_HEAD
} DaiAstBase;
// #endregion

#endif /* CBDAI_DAI_ASTBASE_H */
