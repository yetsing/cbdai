#ifndef BA91B8C9_4E81_413C_886C_26C2FACF07B4
#define BA91B8C9_4E81_413C_886C_26C2FACF07B4

#include "dai_ast/dai_astblockstatement.h"
#include "dai_ast/dai_astexpression.h"

typedef struct {
    DaiAstExpression *condition;
    DaiAstBlockStatement *then_branch;
} DaiBranch;

typedef struct {
    DAI_AST_STATEMENT_HEAD
    DaiAstExpression *condition;
    DaiAstBlockStatement *then_branch;
    DaiAstBlockStatement *else_branch;
} DaiAstIfStatement;

DaiAstIfStatement *
DaiAstIfStatement_New(void);

#endif /* BA91B8C9_4E81_413C_886C_26C2FACF07B4 */
