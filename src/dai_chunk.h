/*
字节码块
*/
#ifndef A93FF060_4FC9_4322_AF9B_CD1FB97FF8A0
#define A93FF060_4FC9_4322_AF9B_CD1FB97FF8A0

#include "dai_common.h"
#include "dai_symboltable.h"
#include "dai_value.h"

typedef struct {
    const char* name;
    int         operand_bytes;   // 操作数的字节数
} DaiOpCodeDefinition;

typedef enum __attribute__((__packed__)) {
    DaiOpConstant = 0,

    DaiOpAdd,
    DaiOpSub,
    DaiOpMul,
    DaiOpDiv,

    DaiOpTrue,
    DaiOpFalse,
    DaiOpNil,
    DaiOpUndefined,

    DaiOpEqual,
    DaiOpNotEqual,
    DaiOpGreaterThan,

    DaiOpMinus,   // example: -1
    DaiOpBang,    // example: !true

    // jump 指令的操作数是相对偏移量
    DaiOpJumpIfFalse,
    DaiOpJump,

    DaiOpPop,

    DaiOpDefineGlobal,
    DaiOpGetGlobal,
    DaiOpSetGlobal,

    DaiOpCall,
    DaiOpReturnValue,
    DaiOpReturn,   // 没有返回值的 return

    DaiOpSetLocal,
    DaiOpGetLocal,

    DaiOpGetBuiltin,

    DaiOpClosure,

    DaiOpGetFree,

    DaiOpClass,
    DaiOpDefineField,         // 实例属性
    DaiOpDefineMethod,        // 实例方法
    DaiOpDefineClassField,    // 类属性
    DaiOpDefineClassMethod,   // 类方法
    DaiOpGetProperty,
    DaiOpSetProperty,
    DaiOpGetSelfProperty,
    DaiOpSetSelfProperty,
    DaiOpGetSuperProperty,
    DaiOpInherit,
} DaiOpCode;

DaiOpCodeDefinition*
dai_opcode_lookup(DaiOpCode op);

const char*
dai_opcode_name(DaiOpCode op);

typedef struct {
    // filename 不归 DaiChunk 所有
    const char*   filename;
    int           count;
    int           capacity;
    uint8_t*      code;
    int*          lines;
    DaiValueArray constants;
#ifdef DISASSEMBLE_VARIABLE_NAME
    char** names;
#endif
} DaiChunk;

void
DaiChunk_init(DaiChunk* chunk, const char* filename);

void
DaiChunk_write(DaiChunk* chunk, uint8_t byte, int line);

void
DaiChunk_writeu16(DaiChunk* chunk, DaiOpCode op, uint16_t operand, int line);

void
DaiChunk_reset(DaiChunk* chunk);

int
DaiChunk_addConstant(DaiChunk* chunk, DaiValue value);

uint8_t
DaiChunk_read(DaiChunk* chunk, int offset);
uint16_t
DaiChunk_readu16(DaiChunk* chunk, int offset);

int
DaiChunk_getLine(DaiChunk* chunk, int offset);

#ifdef DISASSEMBLE_VARIABLE_NAME
void
DaiChunk_addName(DaiChunk* chunk, const char* name, int back);
const char*
DaiChunk_getName(DaiChunk* chunk, int offset);
#endif

#endif /* A93FF060_4FC9_4322_AF9B_CD1FB97FF8A0 */
