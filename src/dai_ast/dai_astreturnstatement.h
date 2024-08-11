//
// Created by  on 2024/6/5.
//

#ifndef CBDAI_SRC_DAI_AST_DAI_ASTRETURNSTATEMENT_H_
#define CBDAI_SRC_DAI_AST_DAI_ASTRETURNSTATEMENT_H_

#include "dai_ast/dai_astexpression.h"
#include "dai_ast/dai_aststatement.h"

typedef struct {
  DAI_AST_STATEMENT_HEAD
  DaiAstExpression *return_value;
} DaiAstReturnStatement;

DaiAstReturnStatement *
DaiAstReturnStatement_New(DaiAstExpression *return_value);
#endif //CBDAI_SRC_DAI_AST_DAI_ASTRETURNSTATEMENT_H_
