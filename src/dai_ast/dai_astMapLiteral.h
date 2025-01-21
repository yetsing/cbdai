#ifndef DAI_ASTMAPLITERAL_H
#define DAI_ASTMAPLITERAL_H

#include "dai_ast/dai_astexpression.h"
#include "dai_tokenize.h"

typedef struct {
    DaiAstExpression* key;
    DaiAstExpression* value;
} DaiAstMapLiteralPair;

typedef struct {
    DAI_AST_EXPRESSION_HEAD
    size_t length;
    DaiAstMapLiteralPair* pairs;
} DaiAstMapLiteral;

DaiAstMapLiteral*
DaiAstMapLiteral_New(void);


#endif   // DAI_ASTMAPLITERAL_H
