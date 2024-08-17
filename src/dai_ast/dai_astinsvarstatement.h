#ifndef DBC180D2_54E7_4098_ACBC_35C189C6B529
#define DBC180D2_54E7_4098_ACBC_35C189C6B529

#include "dai_ast/dai_astidentifier.h"
#include "dai_ast/dai_aststatement.h"

typedef struct {
    DAI_AST_STATEMENT_HEAD
    DaiAstIdentifier* name;
    DaiAstExpression* value;
} DaiAstInsVarStatement;

DaiAstInsVarStatement*
DaiAstInsVarStatement_New(DaiAstIdentifier* name, DaiAstExpression* value);


#endif /* DBC180D2_54E7_4098_ACBC_35C189C6B529 */
