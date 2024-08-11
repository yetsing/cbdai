
#ifndef CBDAI_SRC_DAI_AST_DAI_ASTCLASSSTATEMENT_H_
#define CBDAI_SRC_DAI_AST_DAI_ASTCLASSSTATEMENT_H_

#include "dai_tokenize.h"
#include "dai_ast/dai_astidentifier.h"
#include "dai_ast/dai_aststatement.h"
#include "dai_ast/dai_astblockstatement.h"

typedef struct {
  DAI_AST_STATEMENT_HEAD
  char *name;
  DaiAstIdentifier *parent;  // 父类
  DaiAstBlockStatement *body;
} DaiAstClassStatement;

DaiAstClassStatement *
DaiAstClassStatement_New(DaiToken *name);

#endif //CBDAI_SRC_DAI_AST_DAI_ASTCLASSSTATEMENT_H_
