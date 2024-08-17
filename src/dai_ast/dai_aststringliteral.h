#ifndef CB57E96B_E733_407C_AA33_5B030EBC6088
#define CB57E96B_E733_407C_AA33_5B030EBC6088

#include "dai_ast/dai_astexpression.h"
#include "dai_tokenize.h"

typedef struct {
    DAI_AST_EXPRESSION_HEAD
    char* value;
} DaiAstStringLiteral;

DaiAstStringLiteral*
DaiAstStringLiteral_New(DaiToken* token);

#endif /* CB57E96B_E733_407C_AA33_5B030EBC6088 */
