#ifndef B7C8EF04_A069_4F6D_9BAA_3E142EC0C20D
#define B7C8EF04_A069_4F6D_9BAA_3E142EC0C20D

typedef struct {
    // chunk 不归 DaiCompiler 所有，仅作为编译器的一部分
    DaiChunk *chunk;

} DaiCompiler;

static void
DaiCompiler_init(DaiCompiler *compiler, DaiChunk *chunk) {
    compiler->chunk = chunk;

}

static bool
DaiCompiler_compile(DaiCompiler *compiler, DaiAstBase *node) {
    return false;
}

static void
DaiCompiler_reset(DaiCompiler *compiler) {
}

static int
DaiCompiler_addConstant(DaiCompiler *compiler, DaiValue value) {
    return DaiChunk_addConstant(compiler->chunk, value);
}

static void
DaiCompiler_emit(DaiCompiler *compiler, DaiOpCode op, int line) {
    DaiChunk_write(compiler->chunk, (uint8_t)op, line);
}

static void
DaiCompiler_emit2(DaiCompiler *compiler, DaiOpCode op, uint16_t operand, int line) {
    DaiChunk_writeu16(compiler->chunk, op, operand, line);
}



#endif /* B7C8EF04_A069_4F6D_9BAA_3E142EC0C20D */
