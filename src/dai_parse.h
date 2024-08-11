#ifndef A18AE638_3885_4BAC_8C5B_AB7622CC7B06
#define A18AE638_3885_4BAC_8C5B_AB7622CC7B06

#include "dai_tokenize.h"
#include "dai_ast.h"
#include "dai_error.h"

DaiSyntaxError *
dai_parse(DaiTokenList *tlist, DaiAstProgram *program);

#endif /* A18AE638_3885_4BAC_8C5B_AB7622CC7B06 */
