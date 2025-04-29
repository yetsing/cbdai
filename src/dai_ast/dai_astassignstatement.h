#ifndef CBDAI_DAI_ASTASSIGNSTATEMENT_H
#define CBDAI_DAI_ASTASSIGNSTATEMENT_H

#include "dai_ast/dai_astexpression.h"
#include "dai_ast/dai_aststatement.h"

typedef struct {
    DAI_AST_STATEMENT_HEAD
    char* operator;
    DaiAstExpression* left;
    DaiAstExpression* value;
} DaiAstAssignStatement;

DaiAstAssignStatement*
DaiAstAssignStatement_New(void);

#endif /* CBDAI_DAI_ASTASSIGNSTATEMENT_H */
