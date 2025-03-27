/*
字节码块
*/
#ifndef A93FF060_4FC9_4322_AF9B_CD1FB97FF8A0
#define A93FF060_4FC9_4322_AF9B_CD1FB97FF8A0

#include "dai_common.h"
#include "dai_value.h"
#include <stdint.h>

#define GLOBAL_MAX 65536   // uint16
#define LOCAL_MAX 256      // uint8

// 二元操作类型，比如加减乘除
typedef enum {
    // BinaryOpAdd,
    // BinaryOpSub,
    // BinaryOpMul,
    // BinaryOpDiv,
    // BinaryOpMod,

    BinaryOpLeftShift,
    BinaryOpRightShift,
    BinaryOpBitwiseAnd,
    BinaryOpBitwiseXor,
    BinaryOpBitwiseOr,

} DaiBinaryOpType;

const char*
DaiBinaryOpTypeToString(DaiBinaryOpType op);

typedef struct {
    const char* name;
    int operand_bytes;   // 操作数的字节数
} DaiOpCodeDefinition;

typedef enum __attribute__((__packed__)) {
    DaiOpConstant = 0,

    DaiOpAdd,
    DaiOpSub,
    DaiOpMul,
    DaiOpDiv,
    DaiOpMod,
    DaiOpBinary,

    DaiOpSubscript,
    DaiOpSubscriptSet,

    DaiOpTrue,
    DaiOpFalse,
    DaiOpNil,
    DaiOpUndefined,
    DaiOpArray,
    DaiOpMap,

    DaiOpEqual,
    DaiOpNotEqual,
    DaiOpGreaterThan,
    DaiOpGreaterEqualThan,

    DaiOpNot,   // not
    // 使用跳转实现 and 和 or 的短路
    DaiOpAndJump,   // and
    DaiOpOrJump,    // or

    DaiOpMinus,        // example: -1
    DaiOpBang,         // example: !true
    DaiOpBitwiseNot,   // example: ~1

    // jump 指令的操作数是相对偏移量
    DaiOpJumpIfFalse,
    DaiOpJump,
    DaiOpJumpBack,   // 往前跳

    DaiOpIterInit,
    DaiOpIterNext,

    DaiOpPop,
    DaiOpPopN,

    DaiOpDefineGlobal,
    DaiOpGetGlobal,
    DaiOpSetGlobal,

    DaiOpCall,
    DaiOpReturnValue,
    DaiOpReturn,   // 没有返回值的 return

    DaiOpSetLocal,
    DaiOpGetLocal,

    DaiOpGetBuiltin,

    DaiOpSetFunctionDefault,   // 设置函数默认值

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
    DaiOpCallMethod,
    DaiOpCallSelfMethod,
    DaiOpCallSuperMethod,

    DaiOpEnd,
} DaiOpCode;

DaiOpCodeDefinition*
dai_opcode_lookup(const DaiOpCode op);

const char*
dai_opcode_name(const DaiOpCode op);

typedef struct {
    // filename 不归 DaiChunk 所有
    const char* filename;
    int count;
    int capacity;
    uint8_t* code;
    int* lines;
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
DaiChunk_write2(DaiChunk* chunk, uint16_t n, int line);

void
DaiChunk_writeu16(DaiChunk* chunk, DaiOpCode op, uint16_t operand, int line);

void
DaiChunk_reset(DaiChunk* chunk);

int
DaiChunk_addConstant(DaiChunk* chunk, DaiValue value);

uint8_t
DaiChunk_read(const DaiChunk* chunk, int offset);
uint16_t
DaiChunk_readu16(const DaiChunk* chunk, int offset);

int
DaiChunk_getLine(const DaiChunk* chunk, int offset);

uint8_t
DaiChunk_pop(DaiChunk* chunk);

#ifdef DISASSEMBLE_VARIABLE_NAME
void
DaiChunk_addName(DaiChunk* chunk, const char* name, int back);
const char*
DaiChunk_getName(const DaiChunk* chunk, int offset);
#endif

#endif /* A93FF060_4FC9_4322_AF9B_CD1FB97FF8A0 */
