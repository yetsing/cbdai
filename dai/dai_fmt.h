/*
    dai formatter
*/
#ifndef CBDAI_DAI_FMT_H
#define CBDAI_DAI_FMT_H

#include "dai_ast.h"
#include <stdbool.h>

char*
dai_fmt(const DaiAstProgram* program, size_t source_len);

bool
dai_ast_program_eq(const DaiAstProgram* source, const DaiAstProgram* target);

#endif /* CBDAI_DAI_FMT_H */
