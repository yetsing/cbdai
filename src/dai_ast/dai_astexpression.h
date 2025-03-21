#ifndef E090B2B1_3D77_4BD2_AED5_053744D3FB81
#define E090B2B1_3D77_4BD2_AED5_053744D3FB81

#include "dai_ast/dai_astbase.h"

typedef struct _DaiAstExpression DaiAstExpression;
typedef char* (*DaiAstExpressionLiteralFn)(DaiAstExpression* ast);

// string_fn 返回节点的字符串表示
// free_fn 释放节点内存
// literal_fn 返回表达式字面量表示（字符串形式）
#define DAI_AST_EXPRESSION_HEAD           \
    DaiAstType type;                      \
    DaiAstStringFn string_fn;             \
    DaiAstFreeFn free_fn;                 \
    DaiAstExpressionLiteralFn literal_fn; \
    int start_line;                       \
    int start_column;                     \
    int end_line;                         \
    int end_column;

typedef struct _DaiAstExpression {
    DAI_AST_EXPRESSION_HEAD
} DaiAstExpression;


#endif /* E090B2B1_3D77_4BD2_AED5_053744D3FB81 */
