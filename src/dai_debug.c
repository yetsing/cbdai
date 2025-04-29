/*
反汇编
*/
#include <stdint.h>
#include <stdio.h>

#include "dai_builtin.h"
#include "dai_chunk.h"
#include "dai_debug.h"

void
DaiChunk_disassemble(DaiChunk* chunk, const char* name, int indent) {
    if (indent > 0) {
        printf("%*s", indent, " ");
    }
    printf("== %s ==\n", name);

    for (int offset = 0; offset < chunk->count;) {
        if (indent > 0) {
            printf("%*s", indent, " ");
        }
        offset = DaiChunk_disassembleInstruction(chunk, offset);
    }
    if (indent > 0) {
        printf("%*s", indent, " ");
    }
    printf("== end %s ==\n\n", name);
}

static int
constant_instruction(const char* name, DaiChunk* chunk, int offset) {
    uint16_t constant = DaiChunk_readu16(chunk, offset + 1);
    printf("%-32s %4d", name, constant);
    // printf(" '");
    // dai_print_value(chunk->constants.values[constant]);
    // printf("'");
    printf("\n");
    return offset + 3;
}

static int
jump_instruction(const char* name, DaiChunk* chunk, int offset) {
    uint16_t jump_offset = DaiChunk_readu16(chunk, offset + 1);
    printf("%-32s %4d -> %d\n", name, jump_offset, offset + 3 + jump_offset);
    return offset + 3;
}

static int
jump_back_instruction(const char* name, DaiChunk* chunk, int offset) {
    uint16_t jump_offset = DaiChunk_readu16(chunk, offset + 1);
    printf("%-32s %4d -> %d\n", name, jump_offset, offset + 3 - jump_offset);
    return offset + 3;
}

static int
simple_instruction(const char* name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

static int
simple_instruction1(const char* name, const DaiChunk* chunk, int offset) {
    uint8_t n = DaiChunk_read(chunk, offset + 1);
    printf("%s %d\n", name, n);
    return offset + 2;
}

static int
simple_instruction2(const char* name, const DaiChunk* chunk, const int offset) {
    uint16_t n = DaiChunk_readu16(chunk, offset + 1);
    printf("%s %d\n", name, n);
    return offset + 3;
}

static int
iter_next_instruction(const char* name, const DaiChunk* chunk, const int offset) {
    uint8_t n           = DaiChunk_read(chunk, offset + 1);
    uint16_t end_offset = DaiChunk_readu16(chunk, offset + 2);
    printf("%s %d %d\n", name, n, end_offset);
    return offset + 4;
}

static int
binary_instruction(const char* name, const DaiChunk* chunk, const int offset) {
    const DaiBinaryOpType t = DaiChunk_read(chunk, offset + 1);
    printf("%s(%s)\n", name, DaiBinaryOpTypeToString(t));
    return offset + 2;
}

static int
global_instruction(const char* name, DaiChunk* chunk, int offset) {
    uint16_t index = DaiChunk_readu16(chunk, offset + 1);
    printf("%-32s %4d", name, index);
#ifdef DISASSEMBLE_VARIABLE_NAME
    printf("  '%s'", DaiChunk_getName(chunk, offset));
#endif

    printf("\n");
    return offset + 3;
}

static int
call_instruction(const char* name, DaiChunk* chunk, int offset) {
    int argCount = DaiChunk_read(chunk, offset + 1);
    printf("%s %d\n", name, argCount);
    return offset + 2;
}

static int
local_instruction(const char* name, DaiChunk* chunk, int offset) {
    int index = DaiChunk_read(chunk, offset + 1);
    printf("%-32s %4d", name, index);
    fflush(stdout);
#ifdef DISASSEMBLE_VARIABLE_NAME
    printf("  '%s'", DaiChunk_getName(chunk, offset));
#endif

    printf("\n");
    return offset + 2;
}

static int
builtin_instruction(const char* name, DaiChunk* chunk, int offset) {
    uint8_t index = DaiChunk_read(chunk, offset + 1);
    printf("%-32s %4d %s\n", name, index, builtin_names[index]);
    return offset + 2;
}

static int
closure_instruction(const char* name, DaiChunk* chunk, int offset) {
    int index = DaiChunk_readu16(chunk, offset + 1);
    int count = DaiChunk_read(chunk, offset + 3);
    printf("%-32s %4d %4d(free variable)\n", name, index, count);
    return offset + 4;
}

static int
free_instruction(const char* name, DaiChunk* chunk, int offset) {
    int index = DaiChunk_read(chunk, offset + 1);
    printf("%-32s %4d", name, index);
    fflush(stdout);
#ifdef DISASSEMBLE_VARIABLE_NAME
    printf("  '%s'", DaiChunk_getName(chunk, offset));
#endif

    printf("\n");
    return offset + 2;
}

static int
property_instruction(const char* name, DaiChunk* chunk, int offset) {
    uint16_t constant = DaiChunk_readu16(chunk, offset + 1);
    printf("%-32s %4d '", name, constant);
    dai_print_value(chunk->constants.values[constant]);
    printf("'\n");
    return offset + 3;
}

static int
property_instruction1(const char* name, DaiChunk* chunk, int offset) {
    uint16_t constant = DaiChunk_readu16(chunk, offset + 1);
    uint8_t is_const  = DaiChunk_read(chunk, offset + 3);
    printf("%-32s %4d '", name, constant);
    dai_print_value(chunk->constants.values[constant]);
    printf("' %s\n", is_const ? "const" : "var");
    return offset + 4;
}

static int
call_method_instruction(const char* name, DaiChunk* chunk, int offset) {
    uint16_t constant = DaiChunk_readu16(chunk, offset + 1);
    int argCount      = DaiChunk_read(chunk, offset + 1);
    printf("%-32s %4d %d '", name, constant, argCount);
    dai_print_value(chunk->constants.values[constant]);
    printf("'\n");
    return offset + 4;
}

int
DaiChunk_disassembleInstruction(DaiChunk* chunk, int offset) {
    printf("%04d ", offset);

    DaiOpCode instruction = chunk->code[offset];
    switch (instruction) {
        case DaiOpConstant: return constant_instruction("OP_CONSTANT", chunk, offset);

        case DaiOpAdd: return simple_instruction("OP_ADD", offset);
        case DaiOpSub: return simple_instruction("OP_SUB", offset);
        case DaiOpMul: return simple_instruction("OP_MUL", offset);
        case DaiOpDiv: return simple_instruction("OP_DIV", offset);
        case DaiOpMod: return simple_instruction("OP_MOD", offset);
        case DaiOpBinary: return binary_instruction("OP_BINARY", chunk, offset);

        case DaiOpSubscript: return simple_instruction("OP_SUBSCRIPT", offset);
        case DaiOpSubscriptSet: return simple_instruction("OP_SUBSCRIPT_SET", offset);

        case DaiOpTrue: return simple_instruction("OP_TRUE", offset);
        case DaiOpFalse: return simple_instruction("OP_FALSE", offset);
        case DaiOpNil: return simple_instruction("OP_NIL", offset);
        case DaiOpUndefined: return simple_instruction("OP_UNDEFINED", offset);
        case DaiOpArray: return simple_instruction2("OP_ARRAY", chunk, offset);
        case DaiOpMap: return simple_instruction2("OP_MAP", chunk, offset);

        case DaiOpEqual: return simple_instruction("OP_EQUAL", offset);
        case DaiOpNotEqual: return simple_instruction("OP_NOT_EQUAL", offset);
        case DaiOpGreaterThan: return simple_instruction("OP_GREATER_THAN", offset);
        case DaiOpGreaterEqualThan: return simple_instruction("OP_GREATER_EQUAL_THAN", offset);

        case DaiOpNot: return simple_instruction("OP_NOT", offset);
        case DaiOpAndJump: return jump_instruction("OP_AND_JUMP", chunk, offset);
        case DaiOpOrJump: return jump_instruction("OP_OR_JUMP", chunk, offset);

        case DaiOpMinus: return simple_instruction("OP_MINUS", offset);
        case DaiOpBang: return simple_instruction("OP_BANG", offset);
        case DaiOpBitwiseNot: return simple_instruction("OP_BITWISE_NOT", offset);

        case DaiOpJumpIfFalse: return jump_instruction("OP_JUMP_IF_FALSE", chunk, offset);
        case DaiOpJump: return jump_instruction("OP_JUMP", chunk, offset);
        case DaiOpJumpBack: return jump_back_instruction("OP_JUMP_BACK", chunk, offset);

        case DaiOpIterInit: return simple_instruction1("OP_ITER", chunk, offset);
        case DaiOpIterNext: return iter_next_instruction("OP_ITER_NEXT", chunk, offset);

        case DaiOpPop: return simple_instruction("OP_POP", offset);
        case DaiOpPopN: return simple_instruction1("OP_POP_N", chunk, offset);

        case DaiOpDefineGlobal: return global_instruction("OP_DEFINE_GLOBAL", chunk, offset);
        case DaiOpSetGlobal: return global_instruction("OP_SET_GLOBAL", chunk, offset);
        case DaiOpGetGlobal: return global_instruction("OP_GET_GLOBAL", chunk, offset);

        case DaiOpCall: return call_instruction("OP_CALL", chunk, offset);
        case DaiOpReturnValue: return simple_instruction("OP_RETURN_VALUE", offset);
        case DaiOpReturn: return simple_instruction("OP_RETURN", offset);

        case DaiOpSetLocal: return local_instruction("OP_SET_LOCAL", chunk, offset);
        case DaiOpGetLocal: return local_instruction("OP_GET_LOCAL", chunk, offset);

        case DaiOpGetBuiltin: return builtin_instruction("OP_GET_BUILTIN", chunk, offset);

        case DaiOpSetFunctionDefault:
            return simple_instruction1("OP_SET_FUNCTION_DEFAULT", chunk, offset);

        case DaiOpClosure: return closure_instruction("OP_CLOSURE", chunk, offset);

        case DaiOpGetFree: return free_instruction("OP_GET_FREE", chunk, offset);

        case DaiOpClass: return constant_instruction("OP_CLASS", chunk, offset);
        case DaiOpDefineField: return property_instruction1("OP_DEFINE_FIELD", chunk, offset);
        case DaiOpDefineMethod: return property_instruction("OP_DEFINE_METHOD", chunk, offset);
        case DaiOpDefineClassField:
            return property_instruction1("OP_DEFINE_CLASS_FIELD", chunk, offset);
        case DaiOpDefineClassMethod:
            return property_instruction("OP_DEFINE_CLASS_METHOD", chunk, offset);
        case DaiOpGetProperty: return property_instruction("OP_GET_PROPERTY", chunk, offset);
        case DaiOpSetProperty: return property_instruction("OP_SET_PROPERTY", chunk, offset);
        case DaiOpGetSelfProperty:
            return property_instruction("OP_GET_SELF_PROPERTY", chunk, offset);
        case DaiOpSetSelfProperty:
            return property_instruction("OP_SET_SELF_PROPERTY", chunk, offset);
        case DaiOpGetSuperProperty:
            return property_instruction("OP_GET_SUPER_PROPERTY", chunk, offset);
        case DaiOpInherit: return simple_instruction("OP_INHERIT", offset);
        case DaiOpCallMethod: return call_method_instruction("OP_CALL_METHOD", chunk, offset);
        case DaiOpCallSelfMethod:
            return call_method_instruction("OP_CALL_SELF_METHOD", chunk, offset);
        case DaiOpCallSuperMethod:
            return call_method_instruction("OP_CALL_SUPER_METHOD", chunk, offset);

        case DaiOpEnd: return simple_instruction("OP_END", offset);
    }
    return offset + 1;
}