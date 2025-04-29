#ifndef CBDAI_DAI_COMPILE_H
#define CBDAI_DAI_COMPILE_H

#include "dai_ast.h"
#include "dai_ast/dai_astprogram.h"
#include "dai_error.h"
#include "dai_object.h"
#include "dai_vm.h"

DaiCompileError*
dai_compile(DaiAstProgram* program, DaiObjModule* module, DaiVM* vm);

#endif /* CBDAI_DAI_COMPILE_H */
