#ifndef CBDAI_SRC_DAI_AST_DAI_ASTSUPEREXPRESSION_H
#define CBDAI_SRC_DAI_AST_DAI_ASTSUPEREXPRESSION_H

#include "dai_ast/dai_astexpression.h"
#include "dai_ast/dai_astidentifier.h"

typedef struct {
    DAI_AST_EXPRESSION_HEAD
    DaiAstIdentifier* name;
} DaiAstSuperExpression;

DaiAstSuperExpression*
DaiAstSuperExpression_New(void);

#endif /* CBDAI_SRC_DAI_AST_DAI_ASTSUPEREXPRESSION_H */
