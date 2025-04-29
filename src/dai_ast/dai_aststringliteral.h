#ifndef CBDAI_DAI_ASTSTRINGLITERAL_H
#define CBDAI_DAI_ASTSTRINGLITERAL_H

#include "dai_ast/dai_astexpression.h"
#include "dai_tokenize.h"

typedef struct {
    DAI_AST_EXPRESSION_HEAD
    char* value;
} DaiAstStringLiteral;

DaiAstStringLiteral*
DaiAstStringLiteral_New(DaiToken* token);

#endif /* CBDAI_DAI_ASTSTRINGLITERAL_H */
