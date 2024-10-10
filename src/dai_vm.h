#ifndef C9146535_C1FA_42E3_B5A4_60209E40BC53
#define C9146535_C1FA_42E3_B5A4_60209E40BC53

#include "dai_chunk.h"
#include "dai_error.h"
#include "dai_table.h"
#include "dai_value.h"

#define GLOBAL_MAX 65536
#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef enum {
    VMState_pending,
    VMState_compiling,
    VMState_running,
} VMState;

typedef DaiValue (*VMCallback)(DaiVM* vm);

typedef struct {
    DaiObjFunction* function;
    DaiObjClosure* closure;
    uint8_t* ip;
    DaiValue* slots;             // 局部变量存放位置
    VMCallback returnCallback;   // 函数 return 时的回调，回调返回的值会作为函数返回值
} CallFrame;

extern DaiValue dai_true;
extern DaiValue dai_false;


typedef struct _DaiVM {
    CallFrame frames[FRAMES_MAX];
    int frameCount;

    DaiChunk* chunk;
    uint8_t* ip;   // 指向下一条指令
    DaiValue stack[STACK_MAX];
    DaiValue* stack_top;     // 指向栈顶的下一个位置
    DaiTable strings;        // 字符串驻留
    size_t bytesAllocated;   // 虚拟机管理的内存字节数
    size_t nextGC;           // 下一次 GC 的阈值
    DaiObj* objects;

    int grayCount;
    int grayCapacity;
    DaiObj** grayStack;

    // 全局变量和全局符号表
    DaiSymbolTable* globalSymbolTable;
    DaiValue* globals;

    VMState state;


    // 内置函数
    DaiValue builtin_funcs[256];
} DaiVM;

void
DaiVM_init(DaiVM* vm);
void
DaiVM_reset(DaiVM* vm);
void
DaiVM_resetStack(DaiVM* vm);
DaiRuntimeError*
DaiVM_run(DaiVM* vm, DaiObjFunction* function);

DaiValue
DaiVM_stackTop(const DaiVM* vm);

// #region 用于测试的函数
DaiValue
DaiVM_lastPopedStackElem(const DaiVM* vm);
bool
DaiVM_isEmptyStack(const DaiVM* vm);


// #endregion


#endif /* C9146535_C1FA_42E3_B5A4_60209E40BC53 */
