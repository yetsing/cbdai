#ifndef CBDAI_DAI_ASTCONTINUESTATEMENT_H
#define CBDAI_DAI_ASTCONTINUESTATEMENT_H

#include "dai_ast/dai_aststatement.h"

typedef struct {
    DAI_AST_STATEMENT_HEAD
} DaiAstContinueStatement;

DaiAstContinueStatement*
DaiAstContinueStatement_New(void);

#endif /* CBDAI_DAI_ASTCONTINUESTATEMENT_H */
