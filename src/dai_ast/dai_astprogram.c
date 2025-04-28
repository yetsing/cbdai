#include <assert.h>
#include <string.h>

#include "dai_ast/dai_astcommon.h"
#include "dai_ast/dai_astprogram.h"
#include "dai_malloc.h"
#include "dai_stringbuffer.h"
#include "dai_tokenize.h"

static char*
DaiAstProgram_string(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_program);
    DaiAstProgram* prog = (DaiAstProgram*)base;
    DaiStringBuffer* sb = DaiStringBuffer_New();
    DaiStringBuffer_write(sb, "{\n");
    DaiStringBuffer_write(sb, indent);
    // DaiStringBuffer_write(sb, "type: DaiAstType_program,\n");
    DaiStringBuffer_write(sb, KEY_COLOR("type") ": " TYPE_COLOR("DaiAstType_program") ",\n");
    DaiStringBuffer_write(sb, indent);
    DaiStringBuffer_write(sb, "length: ");
    DaiStringBuffer_writeInt(sb, (int)prog->length);
    DaiStringBuffer_write(sb, ",\n");
    if (recursive) {
        // 递归构建字符串
        DaiStringBuffer_write(sb, indent);
        DaiStringBuffer_write(sb, "statements: [\n");
        DaiAstStatement** statements = prog->statements;
        for (size_t i = 0; i < prog->length; i++) {
            char* s = statements[i]->string_fn((DaiAstBase*)statements[i], true);
            DaiStringBuffer_writeWithLinePrefix(sb, s, doubleindent);
            DaiStringBuffer_write(sb, ",\n");
            dai_free(s);
        }
        DaiStringBuffer_write(sb, indent);
        DaiStringBuffer_write(sb, "],\n");
    } else {
        DaiStringBuffer_write(sb, indent);
        DaiStringBuffer_write(sb, "statements: [...],\n");
    }
    DaiStringBuffer_write(sb, "}\n");
    return DaiStringBuffer_getAndFree(sb, NULL);
}

static void
DaiAstProgram_free(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_program);
    DaiAstProgram* prog = (DaiAstProgram*)base;
    if (recursive) {
        // 递归释放整个 ast 树
        DaiAstStatement** statements = prog->statements;
        for (size_t i = 0; i < prog->length; i++) {
            statements[i]->free_fn((DaiAstBase*)statements[i], true);
        }
    }
    dai_free(prog->statements);
    DaiTokenList_reset(&prog->tlist);
    // program 本身会放在栈上，所以不需要释放
    // dai_free(prog);
    // 重新初始化一遍，防止意外使用
    DaiAstProgram_init(prog);
};

void
DaiAstProgram_init(DaiAstProgram* program) {
    program->type       = DaiAstType_program;
    program->string_fn  = DaiAstProgram_string;
    program->free_fn    = DaiAstProgram_free;
    program->length     = 0;
    program->size       = 0;
    program->statements = NULL;
    DaiTokenList_init(&program->tlist);
}

void
DaiAstProgram_reset(DaiAstProgram* program) {
    DaiAstStatement** statements = program->statements;
    for (size_t i = 0; i < program->length; i++) {
        statements[i]->free_fn((DaiAstBase*)statements[i], true);
    }
    dai_free(program->statements);
    DaiTokenList_reset(&program->tlist);
    DaiAstProgram_init(program);
}

void
DaiAstProgram_append(DaiAstProgram* program, DaiAstStatement* stmt) {
    if (program->length + 1 > program->size) {
        size_t newsize      = program->size < 8 ? 8 : program->size * 2;
        program->statements = dai_realloc(program->statements, sizeof(DaiAstStatement*) * newsize);
        program->size       = newsize;
    }
    program->statements[program->length] = stmt;
    program->length++;
}
