#ifndef DF4D362D_3C93_4246_84BA_501639E74670
#define DF4D362D_3C93_4246_84BA_501639E74670

#include "dai_ast/dai_astblockstatement.h"
#include "dai_ast/dai_astexpression.h"
#include "dai_ast/dai_aststatement.h"

typedef struct {
    DAI_AST_STATEMENT_HEAD
    DaiAstExpression*     condition;
    DaiAstBlockStatement* body;
} DaiAstWhileStatement;

DaiAstWhileStatement*
DaiAstWhileStatement_New(void);

#endif /* DF4D362D_3C93_4246_84BA_501639E74670 */
