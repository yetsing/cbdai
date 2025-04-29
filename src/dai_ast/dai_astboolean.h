#ifndef CBDAI_DAI_ASTBOOLEAN_H
#define CBDAI_DAI_ASTBOOLEAN_H

#include <stdbool.h>

#include "dai_ast/dai_astexpression.h"
#include "dai_tokenize.h"

typedef struct {
    DAI_AST_EXPRESSION_HEAD
    bool value;
} DaiAstBoolean;

DaiAstBoolean*
DaiAstBoolean_New(DaiToken* token);


#endif /* CBDAI_DAI_ASTBOOLEAN_H */
