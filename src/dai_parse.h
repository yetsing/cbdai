#ifndef CBDAI_DAI_PARSE_H
#define CBDAI_DAI_PARSE_H

#include "dai_ast.h"
#include "dai_ast/dai_astprogram.h"
#include "dai_error.h"

DaiSyntaxError*
dai_parse(const char* text, const char* filename, DaiAstProgram* program);

#endif /* CBDAI_DAI_PARSE_H */
