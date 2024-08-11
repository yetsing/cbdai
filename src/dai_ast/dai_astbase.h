#ifndef A1A11555_07DC_49E9_AD46_766F7F9A7043
#define A1A11555_07DC_49E9_AD46_766F7F9A7043

#include <stdbool.h>

#include "dai_ast/dai_asttype.h"

//#region DaiAst 基类
typedef struct _DaiAstBase DaiAstBase;
// 每个 ast 节点都有下面这两个函数
// 返回 ast 节点的字符串，方便查看和调试
// 字符串是分配在堆上的，需要调用者自己释放
typedef char * (*DaiAstStringFn) (DaiAstBase *ast, bool recursive);
// 释放 ast 节点
typedef void (*DaiAstFreeFn) (DaiAstBase *ast, bool recursive);

// ast 节点公共部分，每个 ast 结构体的前面都要有这个部分
#define DAI_AST_BASE_HEAD \
    DaiAstType type;     \
    DaiAstStringFn string_fn; \
    DaiAstFreeFn free_fn;

typedef struct _DaiAstBase {
    DAI_AST_BASE_HEAD
} DaiAstBase;
//#endregion

#endif /* A1A11555_07DC_49E9_AD46_766F7F9A7043 */
