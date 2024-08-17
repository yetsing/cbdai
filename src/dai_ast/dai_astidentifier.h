#ifndef A98A0BCC_FE15_40BF_B927_5BB9A3DD92A0
#define A98A0BCC_FE15_40BF_B927_5BB9A3DD92A0

#include "dai_ast/dai_astexpression.h"
#include "dai_tokenize.h"

typedef struct {
    DAI_AST_EXPRESSION_HEAD
    char* value;
} DaiAstIdentifier;

DaiAstIdentifier*
DaiAstIdentifier_New(DaiToken* token);


#endif /* A98A0BCC_FE15_40BF_B927_5BB9A3DD92A0 */
