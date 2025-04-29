#ifndef CBDAI_DAI_ASTFUNCTIONLITERAL_H
#define CBDAI_DAI_ASTFUNCTIONLITERAL_H

#include <stddef.h>

#include "dai_array.h"
#include "dai_ast/dai_astblockstatement.h"
#include "dai_ast/dai_astidentifier.h"

typedef struct {
    DAI_AST_EXPRESSION_HEAD
    size_t parameters_count;
    DaiAstIdentifier** parameters;
    DaiArray* defaults;   // DaiAstExpression* array
    DaiAstBlockStatement* body;
} DaiAstFunctionLiteral;

DaiAstFunctionLiteral*
DaiAstFunctionLiteral_New(void);

#endif /* CBDAI_DAI_ASTFUNCTIONLITERAL_H */
