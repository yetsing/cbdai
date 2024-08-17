#ifndef CBDAI_SRC_DAI_AST_DAI_ASTSELFEXPRESSION_H
#define CBDAI_SRC_DAI_AST_DAI_ASTSELFEXPRESSION_H

#include "dai_ast/dai_astexpression.h"
#include "dai_ast/dai_astidentifier.h"

typedef struct {
    DAI_AST_EXPRESSION_HEAD
    DaiAstIdentifier* name;
} DaiAstSelfExpression;

DaiAstSelfExpression*
DaiAstSelfExpression_New(void);

#endif /* CBDAI_SRC_DAI_AST_DAI_ASTSELFEXPRESSION_H */
