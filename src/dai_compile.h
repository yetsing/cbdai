#ifndef FF0B206A_5815_45E6_9FEC_1A45F4287F56
#define FF0B206A_5815_45E6_9FEC_1A45F4287F56

#include "dai_ast.h"
#include "dai_ast/dai_astprogram.h"
#include "dai_error.h"
#include "dai_object.h"
#include "dai_vm.h"

// NOTE: 函数内会释放 program 的内存
// DaiCompileError*
// dai_compile(DaiAstProgram* program, DaiObjFunction* funcion, DaiVM* vm);

DaiCompileError*
dai_compile(DaiAstProgram* program, DaiObjModule* module, DaiVM* vm);

#endif /* FF0B206A_5815_45E6_9FEC_1A45F4287F56 */
