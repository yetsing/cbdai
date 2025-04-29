#ifndef CBDAI_DAI_ASTWHILESTATEMENT_H
#define CBDAI_DAI_ASTWHILESTATEMENT_H

#include "dai_ast/dai_astblockstatement.h"
#include "dai_ast/dai_astexpression.h"
#include "dai_ast/dai_aststatement.h"

typedef struct {
    DAI_AST_STATEMENT_HEAD
    DaiAstExpression* condition;
    DaiAstBlockStatement* body;
} DaiAstWhileStatement;

DaiAstWhileStatement*
DaiAstWhileStatement_New(void);

#endif /* CBDAI_DAI_ASTWHILESTATEMENT_H */
