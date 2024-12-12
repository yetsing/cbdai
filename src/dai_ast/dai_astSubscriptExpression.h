#ifndef CBDAI_SRC_DAI_AST_DAI_ASTSUBSCRIPTEXPRESSION_H
#define CBDAI_SRC_DAI_AST_DAI_ASTSUBSCRIPTEXPRESSION_H

#include "dai_ast/dai_astexpression.h"
#include "dai_tokenize.h"

// example: x[3] left is x, right is 3
typedef struct {
    DAI_AST_EXPRESSION_HEAD
    DaiAstExpression* left;
    DaiAstExpression* right;
} DaiAstSubscriptExpression;

DaiAstSubscriptExpression*
DaiAstSubscriptExpression_New(DaiAstExpression* left);

#endif /* CBDAI_SRC_DAI_AST_DAI_ASTSUBSCRIPTEXPRESSION_H */
