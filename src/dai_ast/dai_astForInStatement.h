#ifndef CBDAI_DAI_ASTFORINSTATEMENT_H
#define CBDAI_DAI_ASTFORINSTATEMENT_H

#include "dai_ast/dai_astblockstatement.h"
#include "dai_ast/dai_astexpression.h"
#include "dai_ast/dai_astidentifier.h"
#include "dai_ast/dai_aststatement.h"

typedef struct {
    DAI_AST_STATEMENT_HEAD
    // for (var i, e in expression) body
    // i e 是 for-in 循环的变量
    DaiAstIdentifier* i;
    DaiAstIdentifier* e;
    DaiAstExpression* expression;
    DaiAstBlockStatement* body;
} DaiAstForInStatement;

DaiAstForInStatement*
DaiAstForInStatement_New(void);

#endif /* CBDAI_DAI_ASTFORINSTATEMENT_H */
