//
// Created by  on 2024/6/5.
//

#ifndef CBDAI_SRC_DAI_AST_DAI_ASTVARSTATEMENT_H_
#define CBDAI_SRC_DAI_AST_DAI_ASTVARSTATEMENT_H_

#include "dai_ast/dai_astidentifier.h"
#include "dai_ast/dai_aststatement.h"

typedef struct {
    DAI_AST_STATEMENT_HEAD
    bool is_con;
    DaiAstIdentifier* name;
    DaiAstExpression* value;
} DaiAstVarStatement;

DaiAstVarStatement*
DaiAstVarStatement_New(DaiAstIdentifier* name, DaiAstExpression* value);

#endif   // CBDAI_SRC_DAI_AST_DAI_ASTVARSTATEMENT_H_
