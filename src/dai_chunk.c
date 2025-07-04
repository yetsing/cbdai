/*
字节码块
*/

#include "dai_chunk.h"
#include "dai_memory.h"
#include <stdint.h>

#ifdef DISASSEMBLE_VARIABLE_NAME
#    include <assert.h>
#    include <stdio.h>
#    include <string.h>
#endif

static DaiOpCodeDefinition definitions[] = {
    [DaiOpConstant] =
        {
            .name              = "DaiOpConstant",
            .operand_bytes     = 2,
            .stack_size_change = 1,
        },
    [DaiOpAdd]    = {.name = "DaiOpAdd", .operand_bytes = 0, .stack_size_change = -2 + 1},
    [DaiOpSub]    = {.name = "DaiOpSub", .operand_bytes = 0, .stack_size_change = -2 + 1},
    [DaiOpMul]    = {.name = "DaiOpMul", .operand_bytes = 0, .stack_size_change = -2 + 1},
    [DaiOpDiv]    = {.name = "DaiOpDiv", .operand_bytes = 0, .stack_size_change = -2 + 1},
    [DaiOpMod]    = {.name = "DaiOpMod", .operand_bytes = 0, .stack_size_change = -2 + 1},
    [DaiOpBinary] = {.name = "DaiOpBinary", .operand_bytes = 1, .stack_size_change = -2 + 1},

    [DaiOpSubscript] = {.name = "DaiOpSubscript", .operand_bytes = 0, .stack_size_change = -2 + 1},
    [DaiOpSubscriptSet] = {.name              = "DaiOpSubscriptSet",
                           .operand_bytes     = 0,
                           .stack_size_change = -3},

    [DaiOpTrue]      = {.name = "DaiOpTrue", .operand_bytes = 0, .stack_size_change = 1},
    [DaiOpFalse]     = {.name = "DaiOpFalse", .operand_bytes = 0, .stack_size_change = 1},
    [DaiOpNil]       = {.name = "DaiOpNil", .operand_bytes = 0, .stack_size_change = 1},
    [DaiOpUndefined] = {.name = "DaiOpUndefined", .operand_bytes = 0, .stack_size_change = 1},
    // 操作数: uint16 数组元素数量
    [DaiOpArray] = {.name              = "DaiOpArray",
                    .operand_bytes     = 2,
                    .stack_size_change = STACK_SIZE_CHANGE_DEPENDS_ON_OPERAND},
    // 操作数: uint16 map 元素数量
    [DaiOpMap] = {.name              = "DaiOpMap",
                  .operand_bytes     = 2,
                  .stack_size_change = STACK_SIZE_CHANGE_DEPENDS_ON_OPERAND},

    [DaiOpEqual]       = {.name = "DaiOpEqual", .operand_bytes = 0, .stack_size_change = -2 + 1},
    [DaiOpNotEqual]    = {.name = "DaiOpNotEqual", .operand_bytes = 0, .stack_size_change = -2 + 1},
    [DaiOpGreaterThan] = {.name              = "DaiOpGreaterThan",
                          .operand_bytes     = 0,
                          .stack_size_change = -2 + 1},
    [DaiOpGreaterEqualThan] = {.name              = "DaiOpGreaterEqualThan",
                               .operand_bytes     = 0,
                               .stack_size_change = -2 + 1},

    [DaiOpNot] = {.name = "DaiOpNot", .operand_bytes = 0, .stack_size_change = 0},
    // 操作数：uint16 跳转偏移量
    [DaiOpAndJump] = {.name = "DaiOpAnd", .operand_bytes = 2, .stack_size_change = 0},
    // 操作数：uint16 跳转偏移量
    [DaiOpOrJump] = {.name = "DaiOpOr", .operand_bytes = 2, .stack_size_change = 0},

    [DaiOpMinus]      = {.name = "DaiOpMinus", .operand_bytes = 0, .stack_size_change = 0},
    [DaiOpBang]       = {.name = "DaiOpBang", .operand_bytes = 0, .stack_size_change = 0},
    [DaiOpBitwiseNot] = {.name = "DaiOpBitwiseNot", .operand_bytes = 0, .stack_size_change = 0},

    [DaiOpJumpIfFalse] = {.name = "DaiOpJumpIfFalse", .operand_bytes = 2, .stack_size_change = 0},
    [DaiOpJump]        = {.name = "DaiOpJump", .operand_bytes = 2, .stack_size_change = 0},
    [DaiOpJumpBack]    = {.name = "DaiOpJumpBack", .operand_bytes = 2, .stack_size_change = 0},

    // 操作数：uint8 迭代器的索引
    [DaiOpIterInit] = {.name = "DaiOpIterInit", .operand_bytes = 1, .stack_size_change = -1},
    // 操作数：uint8 迭代器的索引， uint16 循环末尾的偏移量
    [DaiOpIterNext] = {.name = "DaiOpIterNext", .operand_bytes = 3, .stack_size_change = 0},


    [DaiOpPop] = {.name = "DaiOpPop", .operand_bytes = 0, .stack_size_change = -1},
    // 操作数：弹出的个数
    [DaiOpPopN] = {.name              = "DaiOpPopN",
                   .operand_bytes     = 1,
                   .stack_size_change = STACK_SIZE_CHANGE_DEPENDS_ON_OPERAND},

    [DaiOpDefineGlobal] = {.name              = "DaiOpDefineGlobal",
                           .operand_bytes     = 2,
                           .stack_size_change = -1},
    [DaiOpGetGlobal]    = {.name = "DaiOpGetGlobal", .operand_bytes = 2, .stack_size_change = 1},
    [DaiOpSetGlobal]    = {.name = "DaiOpSetGlobal", .operand_bytes = 2, .stack_size_change = -1},

    [DaiOpCall]        = {.name          = "DaiOpCall",
                          .operand_bytes = 1,
                          .stack_size_change =
                              STACK_SIZE_CHANGE_DEPENDS_ON_OPERAND},   // 操作数是函数调用参数个数
    [DaiOpReturnValue] = {.name = "DaiOpReturnValue", .operand_bytes = 0, .stack_size_change = 0},
    [DaiOpReturn]      = {.name = "DaiOpReturn", .operand_bytes = 0, .stack_size_change = 0},

    [DaiOpSetLocal] = {.name = "DaiOpSetLocal", .operand_bytes = 1, .stack_size_change = -1},
    [DaiOpGetLocal] = {.name = "DaiOpGetLocal", .operand_bytes = 1, .stack_size_change = 1},

    [DaiOpGetBuiltin] = {.name = "DaiOpGetBuiltin", .operand_bytes = 1, .stack_size_change = 1},

    // 操作数：uint8 函数默认值的个数
    [DaiOpSetFunctionDefault] = {.name              = "DaiOpSetFunctionDefault",
                                 .operand_bytes     = 1,
                                 .stack_size_change = STACK_SIZE_CHANGE_DEPENDS_ON_OPERAND},

    // 操作数: uint16 函数常量的索引，uint8 自由变量个数
    [DaiOpClosure] = {.name              = "DaiOpClosure",
                      .operand_bytes     = 3,
                      .stack_size_change = STACK_SIZE_CHANGE_DEPENDS_ON_OPERAND},

    [DaiOpGetFree] = {.name = "DaiOpGetFree", .operand_bytes = 1, .stack_size_change = 1},

    // 操作数：类名的常量索引
    [DaiOpClass] = {.name = "DaiOpClass", .operand_bytes = 2, .stack_size_change = 1},
    // 操作数：uint16 属性名的常量索引，uint8 是否 const
    [DaiOpDefineField] = {.name = "DaiOpDefineField", .operand_bytes = 3, .stack_size_change = -1},
    // 操作数：属性名的常量索引
    [DaiOpDefineMethod] = {.name              = "DaiOpDefineMethod",
                           .operand_bytes     = 2,
                           .stack_size_change = -1},
    // 操作数：uint16 属性名的常量索引，uint8 是否 const
    [DaiOpDefineClassField] = {.name              = "DaiOpDefineClassField",
                               .operand_bytes     = 3,
                               .stack_size_change = -1},
    // 操作数：属性名的常量索引
    [DaiOpDefineClassMethod] = {.name              = "DaiOpDefineClassMethod",
                                .operand_bytes     = 2,
                                .stack_size_change = -1},
    // 操作数：属性名的常量索引
    [DaiOpGetProperty] = {.name = "DaiOpGetProperty", .operand_bytes = 2, .stack_size_change = 0},
    // 操作数：属性名的常量索引
    [DaiOpSetProperty] = {.name = "DaiOpSetProperty", .operand_bytes = 2, .stack_size_change = -2},
    // 操作数：属性名的常量索引
    [DaiOpGetSelfProperty] = {.name              = "DaiOpGetSelfProperty",
                              .operand_bytes     = 2,
                              .stack_size_change = 1},
    // 操作数：属性名的常量索引
    [DaiOpSetSelfProperty] = {.name              = "DaiOpSetSelfProperty",
                              .operand_bytes     = 2,
                              .stack_size_change = -1},
    // 操作数：属性名的常量索引
    [DaiOpGetSuperProperty] = {.name              = "DaiOpGetSuperProperty",
                               .operand_bytes     = 2,
                               .stack_size_change = 1},
    [DaiOpInherit]          = {.name = "DaiOpInherit", .operand_bytes = 0, .stack_size_change = -1},
    // 操作数：uint16 方法名的常量索引、uint8 参数个数
    [DaiOpCallMethod] = {.name              = "DaiOpCallMethod",
                         .operand_bytes     = 3,
                         .stack_size_change = STACK_SIZE_CHANGE_DEPENDS_ON_OPERAND},
    // 操作数：uint16 方法名的常量索引、uint8 参数个数
    [DaiOpCallSelfMethod] = {.name              = "DaiOpCallSelfMethod",
                             .operand_bytes     = 3,
                             .stack_size_change = STACK_SIZE_CHANGE_DEPENDS_ON_OPERAND},
    // 操作数：uint16 方法名的常量索引、uint8 参数个数
    [DaiOpCallSuperMethod] = {.name              = "DaiOpCallSuperMethod",
                              .operand_bytes     = 3,
                              .stack_size_change = STACK_SIZE_CHANGE_DEPENDS_ON_OPERAND},

    [DaiOpEnd] = {.name = "DaiOpEnd", .operand_bytes = 0, .stack_size_change = 0},
};

const char*
DaiBinaryOpTypeToString(DaiBinaryOpType op) {
    switch (op) {
        // case BinaryOpAdd: return "+";
        // case BinaryOpSub: return "-";
        // case BinaryOpMul: return "*";
        // case BinaryOpDiv: return "/";
        // case BinaryOpMod: return "%";
        case BinaryOpLeftShift: return "<<";
        case BinaryOpRightShift: return ">>";
        case BinaryOpBitwiseAnd: return "&";
        case BinaryOpBitwiseXor: return "^";
        case BinaryOpBitwiseOr: return "|";
    }
    return "unknown";
}

DaiOpCodeDefinition*
dai_opcode_lookup(const DaiOpCode op) {
    if (op >= 0 && op < sizeof(definitions) / sizeof(definitions[0])) {
        return &definitions[op];
    }
    return NULL;
}

const char*
dai_opcode_name(const DaiOpCode op) {
    const DaiOpCodeDefinition* def = dai_opcode_lookup(op);
    if (def) {
        return def->name;
    }
    return "<unknown>";
}

void
DaiChunk_init(DaiChunk* chunk, const char* filename) {
    chunk->filename = filename;
    chunk->count    = 0;
    chunk->capacity = 0;
    chunk->code     = NULL;
    chunk->lines    = NULL;
    DaiValueArray_init(&chunk->constants);

#ifdef DISASSEMBLE_VARIABLE_NAME
    chunk->names = NULL;
#endif
}

void
DaiChunk_write(DaiChunk* chunk, uint8_t byte, int line) {
    if (chunk->capacity < chunk->count + 1) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code     = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
        chunk->lines    = GROW_ARRAY(int, chunk->lines, oldCapacity, chunk->capacity);
#ifdef DISASSEMBLE_VARIABLE_NAME
        chunk->names = GROW_ARRAY(char*, chunk->names, oldCapacity, chunk->capacity);
#endif
    }

#ifdef DISASSEMBLE_VARIABLE_NAME
    chunk->names[chunk->count] = NULL;
#endif
    chunk->code[chunk->count]  = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

void
DaiChunk_write2(DaiChunk* chunk, uint16_t n, int line) {
    // 按大端序写入
    DaiChunk_write(chunk, (uint8_t)(n >> 8), line);
    DaiChunk_write(chunk, (uint8_t)(n & 0xff), line);
}

void
DaiChunk_writeu16(DaiChunk* chunk, DaiOpCode op, uint16_t operand, int line) {
    DaiChunk_write(chunk, (uint8_t)op, line);
    // 按大端序写入
    DaiChunk_write(chunk, (uint8_t)(operand >> 8), line);
    DaiChunk_write(chunk, (uint8_t)(operand & 0xff), line);
}

void
DaiChunk_reset(DaiChunk* chunk) {
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(int, chunk->lines, chunk->capacity);
    DaiValueArray_reset(&chunk->constants);

#ifdef DISASSEMBLE_VARIABLE_NAME
    for (int i = 0; i < chunk->count; i++) {
        if (chunk->names[i] != NULL) {
            FREE_ARRAY(char, chunk->names[i], strlen(chunk->names[i]) + 1);
        }
    }
    FREE_ARRAY(char*, chunk->names, chunk->capacity);
#endif

    DaiChunk_init(chunk, chunk->filename);
}

int
DaiChunk_addConstant(DaiChunk* chunk, DaiValue value) {
    DaiValueArray_write(&chunk->constants, value);
    return chunk->constants.count - 1;
}

uint8_t
DaiChunk_read(const DaiChunk* chunk, int offset) {
    return chunk->code[offset];
}

uint16_t
DaiChunk_readu16(const DaiChunk* chunk, int offset) {
    uint16_t value = (chunk->code[offset] << 8) | chunk->code[offset + 1];
    return value;
}

int
DaiChunk_getLine(const DaiChunk* chunk, int offset) {
    return chunk->lines[offset];
}

uint8_t
DaiChunk_pop(DaiChunk* chunk) {
    chunk->count--;
    return chunk->code[chunk->count];
}

#ifdef DISASSEMBLE_VARIABLE_NAME
void
DaiChunk_addName(DaiChunk* chunk, const char* name, int back) {
    chunk->names[chunk->count - back] = strdup(name);
}
const char*
DaiChunk_getName(const DaiChunk* chunk, int offset) {
    return chunk->names[offset];
}
#endif
