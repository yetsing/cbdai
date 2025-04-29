#ifndef CBDAI_DAI_ASTINFIXEXPRESSION_H
#define CBDAI_DAI_ASTINFIXEXPRESSION_H

#include "dai_ast/dai_astexpression.h"
#include "dai_tokenize.h"

typedef struct {
    DAI_AST_EXPRESSION_HEAD
    char* operator;
    DaiAstExpression* left;
    DaiAstExpression* right;
} DaiAstInfixExpression;

DaiAstInfixExpression*
DaiAstInfixExpression_New(const DaiToken* tk, DaiAstExpression* left);

#endif /* CBDAI_DAI_ASTINFIXEXPRESSION_H */
