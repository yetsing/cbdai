#ifndef CBDAI_DAI_ASTFLOATLITERAL_H
#define CBDAI_DAI_ASTFLOATLITERAL_H

#include <stdint.h>

#include "dai_ast/dai_astexpression.h"
#include "dai_tokenize.h"

typedef struct {
    DAI_AST_EXPRESSION_HEAD
    char* literal;
    double value;
} DaiAstFloatLiteral;

DaiAstFloatLiteral*
DaiAstFloatLiteral_New(DaiToken* token);

#endif /* CBDAI_DAI_ASTFLOATLITERAL_H */
