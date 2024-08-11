#ifndef CD10A624_072A_4677_89EA_4BAA4F985CF9
#define CD10A624_072A_4677_89EA_4BAA4F985CF9

#include <stdbool.h>

#include "dai_tokenize.h"
#include "dai_ast/dai_astexpression.h"

typedef struct {
    DAI_AST_EXPRESSION_HEAD
    bool value;
} DaiAstBoolean;

DaiAstBoolean *
DaiAstBoolean_New(DaiToken *token);


#endif /* CD10A624_072A_4677_89EA_4BAA4F985CF9 */
