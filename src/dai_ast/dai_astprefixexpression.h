#ifndef CBDAI_SRC_DAI_AST_DAI_ASTPREFIXEXPRESSION_H_
#define CBDAI_SRC_DAI_AST_DAI_ASTPREFIXEXPRESSION_H_

#include "dai_ast/dai_astexpression.h"
#include "dai_tokenize.h"

typedef struct {
    DAI_AST_EXPRESSION_HEAD
    char* operator;
    DaiAstExpression* right;
} DaiAstPrefixExpression;

DaiAstPrefixExpression*
DaiAstPrefixExpression_New(const DaiToken* operator, DaiAstExpression * right);
#endif   // CBDAI_SRC_DAI_AST_DAI_ASTPREFIXEXPRESSION_H_
