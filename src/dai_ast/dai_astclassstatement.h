
#ifndef CBDAI_DAI_ASTCLASSSTATEMENT_H
#define CBDAI_DAI_ASTCLASSSTATEMENT_H

#include "dai_ast/dai_astblockstatement.h"
#include "dai_ast/dai_astidentifier.h"
#include "dai_ast/dai_aststatement.h"
#include "dai_tokenize.h"

typedef struct {
    DAI_AST_STATEMENT_HEAD
    char* name;
    DaiAstIdentifier* parent;   // 父类
    DaiAstBlockStatement* body;
} DaiAstClassStatement;

DaiAstClassStatement*
DaiAstClassStatement_New(DaiToken* name);

#endif /* CBDAI_DAI_ASTCLASSSTATEMENT_H */
