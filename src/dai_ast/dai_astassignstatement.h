#ifndef F2A82CE3_A2B6_424D_A75C_6DC3B57957B2
#define F2A82CE3_A2B6_424D_A75C_6DC3B57957B2

#include "dai_ast/dai_astexpression.h"
#include "dai_ast/dai_aststatement.h"

typedef struct {
    DAI_AST_STATEMENT_HEAD
    DaiAstExpression* left;
    DaiAstExpression* value;
} DaiAstAssignStatement;

DaiAstAssignStatement*
DaiAstAssignStatement_New(void);

#endif /* F2A82CE3_A2B6_424D_A75C_6DC3B57957B2 */
