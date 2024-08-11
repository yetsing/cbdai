//
// Created by  on 2024/6/5.
//

#ifndef CBDAI_SRC_DAI_AST_DAI_ASTVARSTATEMENT_H_
#define CBDAI_SRC_DAI_AST_DAI_ASTVARSTATEMENT_H_

#include "dai_ast/dai_aststatement.h"
#include "dai_ast/dai_astidentifier.h"

typedef struct {
  DAI_AST_STATEMENT_HEAD
  DaiAstIdentifier *name;
  DaiAstExpression *value;
} DaiAstVarStatement;

DaiAstVarStatement *
DaiAstVarStatement_New(DaiAstIdentifier *name, DaiAstExpression *value);

#endif //CBDAI_SRC_DAI_AST_DAI_ASTVARSTATEMENT_H_
