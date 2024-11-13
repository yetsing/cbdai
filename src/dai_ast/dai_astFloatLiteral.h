#ifndef CBDAI_SRC_DAI_AST_DAI_ASTFLOATLITERAL_H_
#define CBDAI_SRC_DAI_AST_DAI_ASTFLOATLITERAL_H_

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

#endif   // CBDAI_SRC_DAI_AST_DAI_ASTFLOATLITERAL_H_
