#ifndef SRC_DAI_ASTNIL_H_
#define SRC_DAI_ASTNIL_H_

#include "dai_ast/dai_astexpression.h"
#include "dai_tokenize.h"

typedef struct {
    DAI_AST_EXPRESSION_HEAD
} DaiAstNil;

DaiAstNil*
DaiAstNil_New(DaiToken* token);


#endif /* SRC_DAI_ASTNIL_H_ */
