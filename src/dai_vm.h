#ifndef C9146535_C1FA_42E3_B5A4_60209E40BC53
#define C9146535_C1FA_42E3_B5A4_60209E40BC53

#include <stdint.h>
#include <stdlib.h>

#include "dai_builtin.h"
#include "dai_chunk.h"
#include "dai_common.h"
#include "dai_object.h"
#include "dai_symboltable.h"
#include "dai_table.h"
#include "dai_value.h"

#define FRAMES_MAX 65
// 保证大于 65536 即可，对应 DaiOpArray 操作数
#define STACK_MAX ((LOCAL_MAX + 32) * FRAMES_MAX)

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
    DaiChunk* chunk;
    DaiValue* slots;       // 局部变量存放位置
    DaiValue* globals;     // 全局变量
    int max_local_count;   // 局部变量最大数量
} CallFrame;

extern DaiValue dai_true;
extern DaiValue dai_false;


typedef struct _DaiVM {
    CallFrame frames[FRAMES_MAX];
    int frameCount;

    DaiChunk* chunk;
    uint8_t* ip;   // 指向下一条指令
    DaiValue stack[STACK_MAX];
    DaiValue* stack_max;
    DaiValue* stack_top;     // 指向栈顶的下一个位置
    DaiTable strings;        // 字符串驻留
    size_t bytesAllocated;   // 虚拟机管理的内存字节数
    size_t nextGC;           // 下一次 GC 的阈值
    DaiObj* objects;

    int grayCount;
    int grayCapacity;
    DaiObj** grayStack;

    // 内置符号表
    DaiSymbolTable* builtinSymbolTable;

    VMState state;

    // 内置对象（模块/类/函数）
    DaiValue builtin_objects[BUILTIN_OBJECT_MAX_COUNT];
    int builtin_objects_count;

    // 临时引用，防止被 GC 回收
    // 一些内置函数会创建一些对象，他既不在栈上，也没有被其他对象引用
    // 为了防止被 GC 回收，这里引用一下
    DaiValue temp_ref;

    // 记录导入的模块
    DaiObjMap* modules;

    uint8_t seed[16];   // 随机数种子
} DaiVM;

void
DaiVM_init(DaiVM* vm);
void
DaiVM_reset(DaiVM* vm);
void
DaiVM_resetStack(DaiVM* vm);
void
DaiVM_addBuiltin(DaiVM* vm, const char* name, DaiValue value);
DaiObjError*
DaiVM_runModule(DaiVM* vm, DaiObjModule* module);
// filename 为绝对路径
DaiObjModule*
DaiVM_getModule(DaiVM* vm, const char* filename);
DaiObjError*
DaiVM_loadModule(DaiVM* vm, const char* text, DaiObjModule* module);
// 传入参数
DaiValue
DaiVM_runCall(DaiVM* vm, DaiValue callee, int argCount, ...);
// 调用参数已经在栈上
DaiValue
DaiVM_runCall2(DaiVM* vm, DaiValue callee, int argCount);
// 打印运行时错误（包含错误调用栈）
void
DaiVM_printError(DaiVM* vm, DaiObjError* err);
// input 用来传入源代码（不在文件中的，比如 repl 中输入的代码）
void
DaiVM_printError2(DaiVM* vm, DaiObjError* err, const char* input);
void
DaiVM_setTempRef(DaiVM* vm, DaiValue value);
const char*
DaiVM_getCurrentFilename(const DaiVM* vm);
void
DaiVM_pauseGC(DaiVM* vm);
void
DaiVM_resumeGC(DaiVM* vm);
void
DaiVM_push1(DaiVM* vm, DaiValue value);
void
DaiVM_getSeed2(DaiVM* vm, uint64_t* seed0, uint64_t* seed1);

// #region 用于测试的函数

DaiValue
DaiVM_lastPopedStackElem(const DaiVM* vm);
bool
DaiVM_isEmptyStack(const DaiVM* vm);


// #endregion


#endif /* C9146535_C1FA_42E3_B5A4_60209E40BC53 */
