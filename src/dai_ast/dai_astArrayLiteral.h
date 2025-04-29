#ifndef CBDAI_DAI_ASTARRAYLITERAL_H
#define CBDAI_DAI_ASTARRAYLITERAL_H

#include "dai_ast/dai_astexpression.h"
#include "dai_tokenize.h"

typedef struct {
    DAI_AST_EXPRESSION_HEAD
    size_t length;
    DaiAstExpression** elements;
} DaiAstArrayLiteral;

DaiAstArrayLiteral*
DaiAstArrayLiteral_New(void);


#endif /* CBDAI_DAI_ASTARRAYLITERAL_H */
