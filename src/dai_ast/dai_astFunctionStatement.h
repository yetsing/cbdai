#ifndef CBDAI_DAI_ASTFUNCTIONSTATEMENT_H
#define CBDAI_DAI_ASTFUNCTIONSTATEMENT_H

#include <stddef.h>

#include "dai_array.h"
#include "dai_ast/dai_astblockstatement.h"
#include "dai_ast/dai_astidentifier.h"

typedef struct {
    DAI_AST_STATEMENT_HEAD
    char* name;
    size_t parameters_count;
    DaiAstIdentifier** parameters;
    DaiArray* defaults;   // DaiAstExpression* array
    DaiAstBlockStatement* body;
} DaiAstFunctionStatement;

DaiAstFunctionStatement*
DaiAstFunctionStatement_New(void);

#endif /* CBDAI_DAI_ASTFUNCTIONSTATEMENT_H */
