#ifndef CBDAI_DAI_ASTEXPRESSION_H
#define CBDAI_DAI_ASTEXPRESSION_H

#include "dai_ast/dai_astbase.h"

typedef struct _DaiAstExpression DaiAstExpression;
typedef char* (*DaiAstExpressionLiteralFn)(DaiAstExpression* ast);

// string_fn 返回节点的字符串表示
// free_fn 释放节点内存
// literal_fn 返回表达式字面量表示（字符串形式）
#define DAI_AST_EXPRESSION_HEAD           \
    DAI_AST_BASE_HEAD                     \
    DaiAstExpressionLiteralFn literal_fn; \
    DaiToken* lparen;                     \
    DaiToken* rparen;

typedef struct _DaiAstExpression {
    DAI_AST_EXPRESSION_HEAD
} DaiAstExpression;

#define DAI_AST_EXPRESSION_INIT(expr) \
    expr->lparen = NULL;              \
    expr->rparen = NULL;


#endif /* CBDAI_DAI_ASTEXPRESSION_H */
