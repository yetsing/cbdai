#ifndef CBDAI_DAI_ASTCALLEXPRESSION_H
#define CBDAI_DAI_ASTCALLEXPRESSION_H

#include <stddef.h>

#include "dai_ast/dai_astexpression.h"

typedef struct {
    DAI_AST_EXPRESSION_HEAD
    DaiAstExpression* function;
    size_t arguments_count;
    DaiAstExpression** arguments;
} DaiAstCallExpression;

DaiAstCallExpression*
DaiAstCallExpression_New(void);

#endif /* CBDAI_DAI_ASTCALLEXPRESSION_H */
