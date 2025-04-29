#ifndef CBDAI_DAI_ASTPREFIXEXPRESSION_H
#define CBDAI_DAI_ASTPREFIXEXPRESSION_H

#include "dai_ast/dai_astexpression.h"
#include "dai_tokenize.h"

typedef struct {
    DAI_AST_EXPRESSION_HEAD
    char* operator;
    DaiAstExpression* right;
} DaiAstPrefixExpression;

DaiAstPrefixExpression*
DaiAstPrefixExpression_New(const DaiToken* operator, DaiAstExpression * right);
#endif /* CBDAI_DAI_ASTPREFIXEXPRESSION_H */
