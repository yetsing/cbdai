#ifndef CBDAI_DAI_ASTINSVARSTATEMENT_H
#define CBDAI_DAI_ASTINSVARSTATEMENT_H

#include "dai_ast/dai_astidentifier.h"
#include "dai_ast/dai_aststatement.h"

typedef struct {
    DAI_AST_STATEMENT_HEAD
    bool is_con;
    DaiAstIdentifier* name;
    DaiAstExpression* value;
} DaiAstInsVarStatement;

DaiAstInsVarStatement*
DaiAstInsVarStatement_New(DaiAstIdentifier* name, DaiAstExpression* value);


#endif /* CBDAI_DAI_ASTINSVARSTATEMENT_H */
