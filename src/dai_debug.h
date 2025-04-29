/*
反汇编
*/
#ifndef CBDAI_DAI_DEBUG_H
#define CBDAI_DAI_DEBUG_H

#include "dai_chunk.h"

void
DaiChunk_disassemble(DaiChunk* chunk, const char* name);

int
DaiChunk_disassembleInstruction(DaiChunk* chunk, int offset);


#endif /* CBDAI_DAI_DEBUG_H */
