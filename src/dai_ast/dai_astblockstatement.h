#ifndef CBDAI_DAI_ASTBLOCKSTATEMENT_H
#define CBDAI_DAI_ASTBLOCKSTATEMENT_H

#include <stddef.h>

#include "dai_ast/dai_aststatement.h"

typedef struct {
    DAI_AST_STATEMENT_HEAD
    size_t size;
    size_t length;
    DaiAstStatement** statements;
} DaiAstBlockStatement;

DaiAstBlockStatement*
DaiAstBlockStatement_New(void);
void
DaiAstBlockStatement_append(DaiAstBlockStatement* block, DaiAstStatement* stmt);

#endif /* CBDAI_DAI_ASTBLOCKSTATEMENT_H */
