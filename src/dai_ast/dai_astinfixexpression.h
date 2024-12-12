#ifndef EC5FB07F_776C_42AF_B3EB_4BD2AEB76E03
#define EC5FB07F_776C_42AF_B3EB_4BD2AEB76E03

#include "dai_ast/dai_astexpression.h"
#include "dai_tokenize.h"

typedef struct {
    DAI_AST_EXPRESSION_HEAD
    char* operator;
    DaiAstExpression* left;
    DaiAstExpression* right;
} DaiAstInfixExpression;

DaiAstInfixExpression*
DaiAstInfixExpression_New(const char* operator, DaiAstExpression * left);

#endif /* EC5FB07F_776C_42AF_B3EB_4BD2AEB76E03 */
