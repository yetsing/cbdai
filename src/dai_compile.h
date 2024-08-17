#ifndef FF0B206A_5815_45E6_9FEC_1A45F4287F56
#define FF0B206A_5815_45E6_9FEC_1A45F4287F56

#include "dai_ast.h"
#include "dai_chunk.h"
#include "dai_common.h"
#include "dai_error.h"
#include "dai_object.h"
#include "dai_symboltable.h"
#include "dai_vm.h"

typedef struct {
    DaiObjFunction* function;
    DaiValueArray*  constants;
} DaiCompileResult;

// NOTE: 函数内会释放 program 的内存
DaiCompileError*
dai_compile(DaiAstProgram* program, DaiObjFunction* funcion, DaiVM* vm);

#endif /* FF0B206A_5815_45E6_9FEC_1A45F4287F56 */
