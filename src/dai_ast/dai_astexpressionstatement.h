//
// Created by  on 2024/6/5.
//

#ifndef CBDAI_DAI_ASTEXPRESSIONSTATEMENT_H
#define CBDAI_DAI_ASTEXPRESSIONSTATEMENT_H

#include "dai_ast/dai_astexpression.h"
#include "dai_ast/dai_aststatement.h"

typedef struct {
    DAI_AST_STATEMENT_HEAD
    DaiAstExpression* expression;
} DaiAstExpressionStatement;

DaiAstExpressionStatement*
DaiAstExpressionStatement_New(DaiAstExpression* expression);
#endif /* CBDAI_DAI_ASTEXPRESSIONSTATEMENT_H */
