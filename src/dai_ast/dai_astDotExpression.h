#ifndef CBDAI_DAI_ASTDOTEXPRESSION_H
#define CBDAI_DAI_ASTDOTEXPRESSION_H

#include "dai_ast/dai_astexpression.h"
#include "dai_ast/dai_astidentifier.h"

typedef struct {
    DAI_AST_EXPRESSION_HEAD
    DaiAstExpression* left;
    DaiAstIdentifier* name;
} DaiAstDotExpression;

DaiAstDotExpression*
DaiAstDotExpression_New(DaiAstExpression* left);

#endif /* CBDAI_DAI_ASTDOTEXPRESSION_H */
