//
// Created by  on 2024/6/5.
//

#ifndef CBDAI_SRC_DAI_AST_DAI_ASTPROGRAM_H_
#define CBDAI_SRC_DAI_AST_DAI_ASTPROGRAM_H_

#include <stddef.h>

#include "dai_ast/dai_aststatement.h"
#include "dai_tokenize.h"

typedef struct {
    DAI_AST_BASE_HEAD
    size_t size;
    size_t length;
    DaiAstStatement** statements;
    DaiTokenList tlist;
} DaiAstProgram;

void
DaiAstProgram_init(DaiAstProgram* program);
void
DaiAstProgram_reset(DaiAstProgram* program);
void
DaiAstProgram_append(DaiAstProgram* program, DaiAstStatement* stmt);
#endif   // CBDAI_SRC_DAI_AST_DAI_ASTPROGRAM_H_
