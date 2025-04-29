#ifndef CBDAI_DAI_ASTIDENTIFIER_H
#define CBDAI_DAI_ASTIDENTIFIER_H

#include "dai_ast/dai_astexpression.h"
#include "dai_tokenize.h"

typedef struct {
    DAI_AST_EXPRESSION_HEAD
    char* value;
} DaiAstIdentifier;

DaiAstIdentifier*
DaiAstIdentifier_New(DaiToken* token);


#endif /* CBDAI_DAI_ASTIDENTIFIER_H */
