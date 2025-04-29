//
// Created by  on 2024/6/5.
//

#ifndef CBDAI_DAI_ASTINTEGERLITERAL_H
#define CBDAI_DAI_ASTINTEGERLITERAL_H

#include <stdint.h>

#include "dai_ast/dai_astexpression.h"
#include "dai_tokenize.h"

typedef struct {
    DAI_AST_EXPRESSION_HEAD
    char* literal;
    int64_t value;
} DaiAstIntegerLiteral;

DaiAstIntegerLiteral*
DaiAstIntegerLiteral_New(DaiToken* token);

#endif /* CBDAI_DAI_ASTINTEGERLITERAL_H */
