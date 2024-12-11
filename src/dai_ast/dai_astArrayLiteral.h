#ifndef DAI_ASTARRAYLITERAL_H
#define DAI_ASTARRAYLITERAL_H

#include "dai_ast/dai_astexpression.h"
#include "dai_tokenize.h"

typedef struct {
    DAI_AST_EXPRESSION_HEAD
    size_t length;
    DaiAstExpression** elements;
} DaiAstArrayLiteral;

DaiAstArrayLiteral*
DaiAstArrayLiteral_New(void);


#endif   // DAI_ASTARRAYLITERAL_H
