#ifndef CBDAI_DAI_ASTBREAKSTATEMENT_H
#define CBDAI_DAI_ASTBREAKSTATEMENT_H

#include "dai_ast/dai_aststatement.h"

typedef struct {
    DAI_AST_STATEMENT_HEAD
} DaiAstBreakStatement;

DaiAstBreakStatement*
DaiAstBreakStatement_New(void);

#endif /* CBDAI_DAI_ASTBREAKSTATEMENT_H */
