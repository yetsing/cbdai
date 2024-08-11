#ifndef C47A9DB8_361D_4D90_8F99_AEFE1461D9DB
#define C47A9DB8_361D_4D90_8F99_AEFE1461D9DB

#include "dai_ast.h"
#include "dai_chunk.h"

typedef struct {
    DaiChunk *chunk;

} DaiCompiler;

void
DaiCompiler_init(DaiCompiler *compiler, DaiChunk *chunk);

bool
DaiCompiler_compile(DaiCompiler *compiler, DaiAstProgram *program);

#endif /* C47A9DB8_361D_4D90_8F99_AEFE1461D9DB */
