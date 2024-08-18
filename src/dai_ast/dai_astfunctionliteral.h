#ifndef C9DE1BB0_1FB9_4F27_9F14_09C7100679C4
#define C9DE1BB0_1FB9_4F27_9F14_09C7100679C4

#include <stddef.h>

#include "dai_ast/dai_astblockstatement.h"
#include "dai_ast/dai_astidentifier.h"

typedef struct {
    DAI_AST_EXPRESSION_HEAD
    size_t parameters_count;
    DaiAstIdentifier** parameters;
    DaiAstBlockStatement* body;
} DaiAstFunctionLiteral;

DaiAstFunctionLiteral*
DaiAstFunctionLiteral_New(void);

#endif /* C9DE1BB0_1FB9_4F27_9F14_09C7100679C4 */
