#ifndef CBDAI_SRC_DAI_AST_DAI_ASTCLASSEXPRESSION_H
#define CBDAI_SRC_DAI_AST_DAI_ASTCLASSEXPRESSION_H

#include "dai_ast/dai_astexpression.h"
#include "dai_ast/dai_astidentifier.h"

typedef struct {
    DAI_AST_EXPRESSION_HEAD
    DaiAstIdentifier* name;
} DaiAstClassExpression;

DaiAstClassExpression*
DaiAstClassExpression_New(void);

#endif /* CBDAI_SRC_DAI_AST_DAI_ASTCLASSEXPRESSION_H */
