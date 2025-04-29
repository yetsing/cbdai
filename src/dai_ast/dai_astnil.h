#ifndef CBDAI_DAI_ASTNIL_H
#define CBDAI_DAI_ASTNIL_H

#include "dai_ast/dai_astexpression.h"
#include "dai_tokenize.h"

typedef struct {
    DAI_AST_EXPRESSION_HEAD
} DaiAstNil;

DaiAstNil*
DaiAstNil_New(DaiToken* token);


#endif /* CBDAI_DAI_ASTNIL_H */
