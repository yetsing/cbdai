#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "dai_builtin.h"
#include "dai_chunk.h"
#include "dai_compile.h"
#include "dai_error.h"
#include "dai_malloc.h"
#include "dai_memory.h"
#include "dai_object.h"
#include "dai_parse.h"
#include "dai_symboltable.h"
#include "dai_tokenize.h"
#include "dai_utils.h"
#include "dai_value.h"
#include "dai_vm.h"

#ifdef DEBUG_TRACE_EXECUTION
#    include "dai_debug.h"
#endif

#define CURRENT_FRAME &(vm->frames[vm->frameCount - 1])

static void
DaiVM_push(DaiVM* vm, DaiValue value);
static DaiValue
DaiVM_pop(DaiVM* vm);
static void
DaiVM_popN(DaiVM* vm, int n);
static DaiObjError*
DaiVM_callValue(DaiVM* vm, const DaiValue callee, const int argCount, const DaiValue receiver);

DaiValue dai_true  = {.type = DaiValueType_bool, .as.boolean = true};
DaiValue dai_false = {.type = DaiValueType_bool, .as.boolean = false};

void
DaiVM_resetStack(DaiVM* vm) {
    vm->stack_top  = vm->stack;
    vm->frameCount = 0;
    vm->stack_max  = vm->stack + STACK_MAX;
}

void
DaiVM_init(DaiVM* vm) {
    DaiVM_resetStack(vm);
    {
        // 初始化栈
        for (int i = 0; i < STACK_MAX; i++) {
            vm->stack[i] = UNDEFINED_VAL;
        }
    }
    vm->objects        = NULL;
    vm->bytesAllocated = 0;
    vm->nextGC         = 1024 * 1024;
    vm->temp_ref       = NIL_VAL;

    vm->grayCount    = 0;
    vm->grayCapacity = 0;
    vm->grayStack    = NULL;

    vm->state = VMState_pending;
    DaiTable_init(&vm->strings);
    vm->builtinSymbolTable = DaiSymbolTable_New();

    // dummy frame
    // 确保始终有一个 frame
    // 原因是为了兼容 cbdai/dai.c:dai_execute 执行函数时，
    // 让他有一个父 frame ，在 return 的时候可以返回到父 frame
    CallFrame* frame       = &vm->frames[vm->frameCount++];
    frame->function        = NULL;
    frame->closure         = NULL;
    frame->chunk           = NULL;
    frame->ip              = NULL;
    frame->slots           = vm->stack_top;
    frame->globals         = NULL;
    frame->max_local_count = 0;

    DaiObjError* err = DaiObjMap_New(vm, NULL, 0, &vm->modules);
    if (err != NULL) {
        dai_error("create modules map error: %s\n", err->message);
        abort();
    }

    if (dai_generate_seed(vm->seed) != 0) {
        dai_error("generate seed error\n");
        abort();
    }

    // 初始化内置函数
    {
        vm->builtin_objects_count = 0;
        for (int i = 0; i < BUILTIN_OBJECT_MAX_COUNT; i++) {
            vm->builtin_objects[i] = UNDEFINED_VAL;
        }
        int count                         = 0;
        DaiBuiltinObject* builtin_objects = init_builtin_objects(vm, &count);
        for (int i = 0; i < count; i++) {
            DaiBuiltinObject bobj = builtin_objects[i];
            DaiVM_addBuiltin(vm, bobj.name, bobj.value);
        }
    }
}

void
DaiVM_reset(DaiVM* vm) {
    DaiSymbolTable_free(vm->builtinSymbolTable);
    vm->builtinSymbolTable = NULL;
    DaiTable_reset(&vm->strings);
    dai_free_objects(vm);
}

void
DaiVM_addBuiltin(DaiVM* vm, const char* name, DaiValue value) {
    if (vm->builtin_objects_count >= BUILTIN_OBJECT_MAX_COUNT) {
        dai_error("too many builtin objects\n");
        abort();
    }
    if (IS_MODULE(value)) {
        builtin_module_setup(AS_MODULE(value));
    }
    // 在符号表中注册内置对象
    DaiSymbolTable_defineBuiltin(vm->builtinSymbolTable, vm->builtin_objects_count, name);
    DaiSymbol symbol;
    bool found = DaiSymbolTable_resolve(vm->builtinSymbolTable, name, &symbol);
    assert(found);
    // 在内置对象数组中注册内置对象
    builtin_names[vm->builtin_objects_count]       = name;
    vm->builtin_objects[vm->builtin_objects_count] = value;
    vm->builtin_objects_count++;
}

// #region DaiVM runtime
static DaiValue
DaiVM_peek(const DaiVM* vm, int distance) {
    return vm->stack_top[-1 - distance];
}

static void
concatenate_string(DaiVM* vm, DaiValue v1, DaiValue v2) {
    DaiObjString* a = (DaiObjString*)AS_OBJ(v1);
    DaiObjString* b = (DaiObjString*)AS_OBJ(v2);

    int length  = a->length + b->length;
    char* chars = VM_ALLOCATE(vm, char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    DaiObjString* result = dai_take_string(vm, chars, length);
    DaiVM_pop(vm);
    DaiVM_pop(vm);
    DaiVM_push(vm, OBJ_VAL(result));
}

static DaiObjError*
DaiVM_call(DaiVM* vm, DaiObjFunction* function, int argCount) {
    if ((argCount < function->arity - function->default_count) || (argCount > function->arity)) {
        return DaiObjError_Newf(vm,
                                "%s() expected %d arguments but got %d",
                                function->name->chars,
                                function->arity,
                                argCount);
    }
    if (vm->frameCount == FRAMES_MAX) {
        return DaiObjError_Newf(vm, "maximum recursion depth exceeded");
    }
    // push default values
    {
        for (int i = argCount; i < function->arity; i++) {
            DaiVM_push(vm, function->defaults[i - (function->arity - function->default_count)]);
        }
    }
    // new frame
    CallFrame* frame = &vm->frames[vm->frameCount++];
    frame->function  = function;
    frame->closure   = NULL;
    frame->chunk     = &function->chunk;
    frame->ip        = function->chunk.code;
    // 前面有一个空位用来放 self ，实际是占了被调用函数本身的位置，会在后续操作中更新成 self
    // (例如 DaiVM_callValue DaiObjType_boundMethod 分支)。
    // 因为帧上面有函数的指针，所以占了也没关系
    frame->slots           = vm->stack_top - function->arity - 1;
    frame->globals         = function->module->globals;
    frame->max_local_count = function->max_local_count;
    vm->stack_top          = frame->slots + frame->max_local_count;   // 预分配局部变量空间
    return NULL;
}

static DaiObjError*
DaiVM_callValue(DaiVM* vm, const DaiValue callee, const int argCount, const DaiValue receiver) {
    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
            case DaiObjType_boundMethod: {
                DaiObjBoundMethod* bound_method = AS_BOUND_METHOD(callee);
                return DaiVM_callValue(
                    vm, OBJ_VAL(bound_method->method), argCount, bound_method->receiver);
            }
            case DaiObjType_class: {
                // 记录调用完成后的栈顶位置
                // 因为 DaiObjClass_call 里面可能会改变 stack_top 位置，所以需要在调用前记录下来
                DaiValue* new_stack_top = vm->stack_top - argCount - 1;
                DaiValue result         = DaiObjClass_call(
                    (DaiObjClass*)AS_OBJ(callee), vm, argCount, vm->stack_top - argCount);
                vm->stack_top = new_stack_top;
                if (DAI_IS_ERROR(result)) {
                    return AS_ERROR(result);
                }
                DaiVM_push(vm, result);
                return NULL;
            }
            case DaiObjType_function: {
                DaiObjError* err = DaiVM_call(vm, (DaiObjFunction*)AS_OBJ(callee), argCount);
                if (!IS_UNDEFINED(receiver)) {
                    CallFrame* frame = CURRENT_FRAME;
                    frame->slots[0]  = receiver;
                }
                return err;
            }
            case DaiObjType_builtinFn: {
                const BuiltinFn func = AS_BUILTINFN(callee)->function;
                DaiVM_pauseGC(vm);
                const DaiValue result =
                    func(vm, DaiVM_peek(vm, argCount), argCount, vm->stack_top - argCount);
                DaiVM_resumeGC(vm);
                vm->stack_top = vm->stack_top - argCount - 1;
                if (DAI_IS_ERROR(result)) {
                    return AS_ERROR(result);
                }
                DaiVM_push(vm, result);
                return NULL;
            }
            case DaiObjType_closure: {
                DaiObjClosure* closure = (DaiObjClosure*)AS_OBJ(callee);
                DaiObjError* err       = DaiVM_call(vm, closure->function, argCount);
                vm->frames[vm->frameCount - 1].closure = closure;
                return err;
            }
            case DaiObjType_cFunction: {
                const BuiltinFn func  = AS_CFUNCTION(callee)->wrapper;
                const DaiValue result = func(vm, callee, argCount, vm->stack_top - argCount);
                vm->stack_top         = vm->stack_top - argCount - 1;
                if (DAI_IS_ERROR(result)) {
                    return AS_ERROR(result);
                }
                DaiVM_push(vm, result);
                return NULL;
            }
            default: break;
        }
    }
    return DaiObjError_Newf(vm, "'%s' object is not callable", dai_value_ts(callee));
}

static bool
DaiVM_executeIntBinary(DaiVM* vm, const DaiBinaryOpType opType, const DaiValue a,
                       const DaiValue b) {
    switch (opType) {
        case BinaryOpLeftShift: {
            DaiVM_push(vm, INTEGER_VAL(AS_INTEGER(a) << AS_INTEGER(b)));
            return true;
        }
        case BinaryOpRightShift: {
            DaiVM_push(vm, INTEGER_VAL(AS_INTEGER(a) >> AS_INTEGER(b)));
            return true;
        }
        case BinaryOpBitwiseAnd: {
            DaiVM_push(vm, INTEGER_VAL(AS_INTEGER(a) & AS_INTEGER(b)));
            return true;
        }
        case BinaryOpBitwiseOr: {
            DaiVM_push(vm, INTEGER_VAL(AS_INTEGER(a) | AS_INTEGER(b)));
            return true;
        }
        case BinaryOpBitwiseXor: {
            DaiVM_push(vm, INTEGER_VAL(AS_INTEGER(a) ^ AS_INTEGER(b)));
            return true;
        }
        default: return false;
    }
}

// 运行当前帧，直至当前帧退出
static DaiObjError*
DaiVM_runCurrentFrame(DaiVM* vm) {
    int current_frame_index = vm->frameCount - 1;
    CallFrame* frame        = &vm->frames[vm->frameCount - 1];
    DaiChunk* chunk         = frame->chunk;
    DaiValue* globals       = frame->globals;

    // 数字运算
#define ARITHMETIC_OPERATION(op)                                                         \
    do {                                                                                 \
        DaiValue b = DaiVM_pop(vm);                                                      \
        DaiValue a = DaiVM_pop(vm);                                                      \
        if (IS_INTEGER(a) && IS_INTEGER(b)) {                                            \
            DaiVM_push(vm, INTEGER_VAL(AS_INTEGER(a) op AS_INTEGER(b)));                 \
        } else if (IS_FLOAT(a) && IS_INTEGER(b)) {                                       \
            DaiVM_push(vm, FLOAT_VAL(AS_FLOAT(a) op(double) AS_INTEGER(b)));             \
        } else if (IS_INTEGER(a) && IS_FLOAT(b)) {                                       \
            DaiVM_push(vm, FLOAT_VAL((double)AS_INTEGER(a) op AS_FLOAT(b)));             \
        } else if (IS_FLOAT(a) && IS_FLOAT(b)) {                                         \
            DaiVM_push(vm, FLOAT_VAL(AS_FLOAT(a) op AS_FLOAT(b)));                       \
        } else {                                                                         \
            return DaiObjError_Newf(vm,                                                  \
                                    "unsupported operand type(s) for %s: '%s' and '%s'", \
                                    #op,                                                 \
                                    dai_value_ts(a),                                     \
                                    dai_value_ts(b));                                    \
        }                                                                                \
    } while (0)

    // 先取值，再自增
#define READ_BYTE() (*frame->ip++)
#define READ_UINT16()                                        \
    DaiChunk_readu16(chunk, (int)(frame->ip - chunk->code)); \
    frame->ip += 2

#ifdef DEBUG_TRACE_EXECUTION
    const char* curr_funcname = NULL;
#endif
    //        while (frame->ip < chunk->code + chunk->count) {
    while (true) {
#ifdef DEBUG_TRACE_EXECUTION
        const char* funcname;
        if (frame->function) {
            funcname = DaiObjFunction_name(frame->function);
        } else {
            funcname = chunk->filename;
        }
        if (curr_funcname != funcname) {
            curr_funcname = funcname;
            dai_log("========== %s =========\n", curr_funcname);
        }
        dai_log("          ");
        for (DaiValue* slot = vm->stack; slot < vm->stack_top; slot++) {
            if (slot != vm->stack && frame->slots == slot) {
                dai_log("  ");
            } else if (slot == frame->slots + frame->max_local_count) {
                dai_log(" - ");
            }
            dai_log("[ \"");
            dai_print_value(*slot);
            dai_log("\" ]");
        }
        if (vm->stack_top == vm->stack) {
            dai_log("<EMPTY>");
        }
        dai_log("\n");
        DaiChunk_disassembleInstruction(chunk, (int)(frame->ip - chunk->code));
#endif
        DaiOpCode op = READ_BYTE();
        switch (op) {
            case DaiOpConstant: {
                uint16_t constant_index = READ_UINT16();
                DaiValue constant       = chunk->constants.values[constant_index];
                DaiVM_push(vm, constant);
                break;
            }
            case DaiOpAdd: {
                DaiValue b = DaiVM_peek(vm, 0);
                DaiValue a = DaiVM_peek(vm, 1);
                if (IS_STRING(a) && IS_STRING(b)) {
                    concatenate_string(vm, a, b);
                    break;
                }
                DaiVM_popN(vm, 2);
                if (IS_INTEGER(a) && IS_INTEGER(b)) {
                    DaiVM_push(vm, INTEGER_VAL(AS_INTEGER(a) + AS_INTEGER(b)));
                } else if (IS_FLOAT(a) && IS_INTEGER(b)) {
                    DaiVM_push(vm, FLOAT_VAL(AS_FLOAT(a) + (double)AS_INTEGER(b)));
                } else if (IS_INTEGER(a) && IS_FLOAT(b)) {
                    DaiVM_push(vm, FLOAT_VAL((double)AS_INTEGER(a) + AS_FLOAT(b)));
                } else if (IS_FLOAT(a) && IS_FLOAT(b)) {
                    DaiVM_push(vm, FLOAT_VAL(AS_FLOAT(a) + AS_FLOAT(b)));
                } else {
                    return DaiObjError_Newf(vm,
                                            "unsupported operand type(s) for %s: '%s' and '%s'",
                                            "+",
                                            dai_value_ts(a),
                                            dai_value_ts(b));
                }
                break;
            }
            case DaiOpSub: {
                ARITHMETIC_OPERATION(-);
                break;
            }
            case DaiOpMul: {
                ARITHMETIC_OPERATION(*);
                break;
            }
            case DaiOpDiv: {
                DaiValue b = DaiVM_pop(vm);
                DaiValue a = DaiVM_pop(vm);
                if (AS_INTEGER(b) == 0) {
                    return DaiObjError_Newf(vm, "division by zero");
                }
                if (IS_INTEGER(a) && IS_INTEGER(b)) {
                    DaiVM_push(vm, INTEGER_VAL(AS_INTEGER(a) / AS_INTEGER(b)));
                } else if (IS_FLOAT(a) && IS_INTEGER(b)) {
                    DaiVM_push(vm, FLOAT_VAL(AS_FLOAT(a) / (double)AS_INTEGER(b)));
                } else if (IS_INTEGER(a) && IS_FLOAT(b)) {
                    DaiVM_push(vm, FLOAT_VAL((double)AS_INTEGER(a) / AS_FLOAT(b)));
                } else if (IS_FLOAT(a) && IS_FLOAT(b)) {
                    DaiVM_push(vm, FLOAT_VAL(AS_FLOAT(a) / AS_FLOAT(b)));
                } else {
                    return DaiObjError_Newf(vm,
                                            "unsupported operand type(s) for /: '%s' and '%s'",
                                            dai_value_ts(a),
                                            dai_value_ts(b));
                }
                break;
            }
            case DaiOpMod: {
                DaiValue b = DaiVM_pop(vm);
                DaiValue a = DaiVM_pop(vm);
                if (IS_INTEGER(a) && IS_INTEGER(b)) {
                    DaiVM_push(vm, INTEGER_VAL(AS_INTEGER(a) % AS_INTEGER(b)));
                } else {
                    return DaiObjError_Newf(vm,
                                            "unsupported operand type(s) for %%: '%s' and '%s'",
                                            dai_value_ts(a),
                                            dai_value_ts(b));
                }
                break;
            }
            case DaiOpBinary: {
                DaiBinaryOpType opType = READ_BYTE();
                DaiValue b             = DaiVM_pop(vm);
                DaiValue a             = DaiVM_pop(vm);
                if (IS_INTEGER(a) && IS_INTEGER(b)) {
                    DaiVM_executeIntBinary(vm, opType, a, b);
                    break;
                } else {
                    return DaiObjError_Newf(vm,
                                            "unsupported operand type(s) for %s: '%s' and '%s'",
                                            DaiBinaryOpTypeToString(opType),
                                            dai_value_ts(a),
                                            dai_value_ts(b));
                }
                break;
            }
            case DaiOpSubscript: {
                // 从栈顶排列为 index, object (object[index])
                DaiValue receiver   = DaiVM_peek(vm, 1);
                SubscriptGetFn func = NULL;
                if (IS_OBJ(receiver)) {
                    func = AS_OBJ(receiver)->operation->subscript_get_func;
                }
                if (func) {
                    DaiValue result = func(vm, receiver, DaiVM_peek(vm, 0));
                    if (DAI_IS_ERROR(result)) {
                        return AS_ERROR(result);
                    }
                    vm->stack_top = vm->stack_top - 2;   // 弹出 index, object
                    DaiVM_push(vm, result);
                } else {
                    return DaiObjError_Newf(
                        vm, "'%s' object is not subscriptable", dai_value_ts(receiver));
                }
                break;
            }
            case DaiOpSubscriptSet: {
                // 从栈顶排列为 index, object, value (object[index] = value)
                DaiValue receiver   = DaiVM_peek(vm, 1);
                SubscriptSetFn func = NULL;
                if (IS_OBJ(receiver)) {
                    func = AS_OBJ(receiver)->operation->subscript_set_func;
                }
                if (func) {
                    DaiValue result = func(vm, receiver, DaiVM_peek(vm, 0), DaiVM_peek(vm, 2));
                    vm->stack_top   = vm->stack_top - 3;   // 弹出 index, object, value
                    if (DAI_IS_ERROR(result)) {
                        return AS_ERROR(result);
                    }
                } else {
                    return DaiObjError_Newf(
                        vm, "'%s' object is not subscriptable", dai_value_ts(receiver));
                }
                break;
            }
            case DaiOpTrue: {
                DaiVM_push(vm, dai_true);
                break;
            }
            case DaiOpFalse: {
                DaiVM_push(vm, dai_false);
                break;
            }
            case DaiOpNil: {
                DaiVM_push(vm, NIL_VAL);
                break;
            }
            case DaiOpUndefined: {
                DaiVM_push(vm, UNDEFINED_VAL);
                break;
            }
            case DaiOpArray: {
                uint16_t length    = READ_UINT16();
                DaiObjArray* array = DaiObjArray_New(vm, vm->stack_top - length, length);
                vm->stack_top -= length;
                DaiVM_push(vm, OBJ_VAL(array));
                break;
            }
            case DaiOpMap: {
                uint16_t length  = READ_UINT16();
                DaiObjMap* map   = NULL;
                DaiObjError* err = DaiObjMap_New(vm, vm->stack_top - length * 2, length, &map);
                if (err != NULL) {
                    return err;
                }
                vm->stack_top -= length * 2;
                DaiVM_push(vm, OBJ_VAL(map));
                break;
            }

            case DaiOpEqual: {
                DaiValue b = DaiVM_pop(vm);
                DaiValue a = DaiVM_pop(vm);
                int ret    = dai_value_equal(a, b);
                if (ret == -1) {
                    return DaiObjError_Newf(vm, "maximum recursion depth exceeded in comparison");
                }
                DaiValue res = BOOL_VAL(ret == 1);
                DaiVM_push(vm, res);
                break;
            }
            case DaiOpNotEqual: {
                DaiValue b = DaiVM_pop(vm);
                DaiValue a = DaiVM_pop(vm);
                int ret    = dai_value_equal(a, b);
                if (ret == -1) {
                    return DaiObjError_Newf(vm, "maximum recursion depth exceeded in comparison");
                }
                DaiValue res = BOOL_VAL(ret == 0);
                DaiVM_push(vm, res);
                break;
            }
            case DaiOpGreaterThan: {
                DaiValue b = DaiVM_pop(vm);
                DaiValue a = DaiVM_pop(vm);
                if (IS_INTEGER(a) && IS_INTEGER(b)) {
                    DaiValue res = BOOL_VAL(AS_INTEGER(a) > AS_INTEGER(b));
                    DaiVM_push(vm, res);
                } else if (IS_INTEGER(a) && IS_FLOAT(b)) {
                    DaiValue res = BOOL_VAL(AS_INTEGER(a) > AS_FLOAT(b));
                    DaiVM_push(vm, res);
                } else if (IS_FLOAT(a) && IS_INTEGER(b)) {
                    DaiValue res = BOOL_VAL(AS_FLOAT(a) > AS_INTEGER(b));
                    DaiVM_push(vm, res);
                } else if (IS_FLOAT(a) && IS_FLOAT(b)) {
                    DaiValue res = BOOL_VAL(AS_FLOAT(a) > AS_FLOAT(b));
                    DaiVM_push(vm, res);
                } else if (IS_STRING(a) && IS_STRING(b)) {
                    int ret      = DaiObjString_cmp(AS_STRING(a), AS_STRING(b));
                    DaiValue res = BOOL_VAL(ret > 0);
                    DaiVM_push(vm, res);
                } else {
                    return DaiObjError_Newf(vm,
                                            "unsupported operand type(s) for >/<: '%s' and '%s'",
                                            dai_value_ts(a),
                                            dai_value_ts(b));
                }
                break;
            }
            case DaiOpGreaterEqualThan: {
                DaiValue b = DaiVM_pop(vm);
                DaiValue a = DaiVM_pop(vm);
                if (IS_INTEGER(a) && IS_INTEGER(b)) {
                    DaiValue res = BOOL_VAL(AS_INTEGER(a) >= AS_INTEGER(b));
                    DaiVM_push(vm, res);
                } else if (IS_INTEGER(a) && IS_FLOAT(b)) {
                    DaiValue res = BOOL_VAL(AS_INTEGER(a) >= AS_FLOAT(b));
                    DaiVM_push(vm, res);
                } else if (IS_FLOAT(a) && IS_INTEGER(b)) {
                    DaiValue res = BOOL_VAL(AS_FLOAT(a) >= AS_INTEGER(b));
                    DaiVM_push(vm, res);
                } else if (IS_FLOAT(a) && IS_FLOAT(b)) {
                    DaiValue res = BOOL_VAL(AS_FLOAT(a) >= AS_FLOAT(b));
                    DaiVM_push(vm, res);
                } else if (IS_STRING(a) && IS_STRING(b)) {
                    int ret      = DaiObjString_cmp(AS_STRING(a), AS_STRING(b));
                    DaiValue res = BOOL_VAL(ret >= 0);
                    DaiVM_push(vm, res);
                } else {
                    return DaiObjError_Newf(vm,
                                            "unsupported operand type(s) for >=/<=: '%s' and '%s'",
                                            dai_value_ts(a),
                                            dai_value_ts(b));
                }
                break;
            }

            case DaiOpNot: {
                DaiValue a   = DaiVM_pop(vm);
                DaiValue res = BOOL_VAL(!dai_value_is_truthy(a));
                DaiVM_push(vm, res);
                break;
            }
            case DaiOpAndJump: {
                int offset = READ_UINT16();
                DaiValue a = DaiVM_peek(vm, 0);
                if (!dai_value_is_truthy(a)) {
                    frame->ip += offset;
                }
                break;
            }
            case DaiOpOrJump: {
                int offset = READ_UINT16();
                DaiValue a = DaiVM_peek(vm, 0);
                if (dai_value_is_truthy(a)) {
                    frame->ip += offset;
                } else {
                }
                break;
            }

            case DaiOpMinus: {
                DaiValue a = DaiVM_pop(vm);
                if (IS_INTEGER(a)) {
                    DaiVM_push(vm, INTEGER_VAL(-AS_INTEGER(a)));
                } else if (IS_FLOAT(a)) {
                    DaiVM_push(vm, FLOAT_VAL(-AS_FLOAT(a)));
                } else {
                    return DaiObjError_Newf(
                        vm, "unsupported operand type(s) for -: '%s'", dai_value_ts(a));
                }
                break;
            }
            case DaiOpBang: {
                DaiValue a   = DaiVM_pop(vm);
                DaiValue res = BOOL_VAL(!dai_value_is_truthy(a));
                DaiVM_push(vm, res);
                break;
            }
            case DaiOpBitwiseNot: {
                DaiValue a = DaiVM_pop(vm);
                if (IS_INTEGER(a)) {
                    DaiVM_push(vm, INTEGER_VAL(~AS_INTEGER(a)));
                } else {
                    return DaiObjError_Newf(
                        vm, "unsupported operand type(s) for ~: '%s'", dai_value_ts(a));
                }
                break;
            }

            case DaiOpJumpIfFalse: {
                uint16_t offset = READ_UINT16();
                if (!dai_value_is_truthy(DaiVM_pop(vm))) {
                    frame->ip += offset;
                }
                break;
            }
            case DaiOpJump: {
                uint16_t offset = READ_UINT16();
                frame->ip += offset;
                break;
            }
            case DaiOpJumpBack: {
                uint16_t offset = READ_UINT16();
                frame->ip -= offset;
                break;
            }

            case DaiOpIterInit: {
                uint8_t iterator_slot = READ_BYTE();
                DaiValue val          = DaiVM_peek(vm, 0);   // 先不要 pop ，以免被 GC 回收
                IterInitFn func       = NULL;
                if (IS_OBJ(val)) {
                    func = AS_OBJ(val)->operation->iter_init_func;
                }
                if (func) {
                    DaiValue iterator           = func(vm, val);
                    frame->slots[iterator_slot] = iterator;
                    DaiVM_pop(vm);
                } else {
                    return DaiObjError_Newf(vm, "'%s' object is not iterable", dai_value_ts(val));
                }
                break;
            }
            case DaiOpIterNext: {
                uint8_t iterator_slot = READ_BYTE();
                uint16_t end_offset   = READ_UINT16();
                DaiValue iterator     = frame->slots[iterator_slot];
                DaiValue i, e;
                DaiValue next = AS_OBJ(iterator)->operation->iter_next_func(vm, iterator, &i, &e);
                frame->slots[iterator_slot + 1] = i;
                frame->slots[iterator_slot + 2] = e;
                if (IS_UNDEFINED(next)) {
                    frame->ip += end_offset;
                }
                break;
            }

            case DaiOpPop: {
#ifdef DEBUG_TRACE_EXECUTION
                dai_log("          pop [ ");
                dai_print_value(DaiVM_peek(vm, 0));
                dai_log(" ]\n");
#endif
                DaiVM_pop(vm);
                break;
            }
            case DaiOpPopN: {
                uint8_t n = READ_BYTE();
                DaiVM_popN(vm, n);
                break;
            }

            case DaiOpSetGlobal:
            case DaiOpDefineGlobal: {
                uint16_t globalIndex = READ_UINT16();
                DaiValue val         = DaiVM_pop(vm);
                globals[globalIndex] = val;
                break;
            }
            case DaiOpGetGlobal: {
                uint16_t globalIndex = READ_UINT16();
                DaiValue val         = globals[globalIndex];
                DaiVM_push(vm, val);
                break;
            }

            case DaiOpCall: {
                int argCount     = READ_BYTE();
                DaiValue callee  = DaiVM_peek(vm, argCount);
                DaiObjError* err = DaiVM_callValue(vm, callee, argCount, UNDEFINED_VAL);
                if (err != NULL) {
                    return err;
                }
                frame   = CURRENT_FRAME;
                chunk   = frame->chunk;
                globals = frame->globals;
                break;
            }
            case DaiOpReturnValue: {
                DaiValue result = DaiVM_pop(vm);
                vm->frameCount--;
                vm->stack_top = frame->slots;
                DaiVM_push(vm, result);
                frame   = CURRENT_FRAME;
                chunk   = frame->chunk;
                globals = frame->globals;
                if (vm->frameCount == current_frame_index) {
                    return NULL;
                }
                break;
            }
            case DaiOpReturn: {
                DaiValue result = NIL_VAL;
                vm->frameCount--;
                vm->stack_top = frame->slots;
                DaiVM_push(vm, result);
                frame   = CURRENT_FRAME;
                chunk   = frame->chunk;
                globals = frame->globals;
                if (vm->frameCount == current_frame_index) {
                    return NULL;
                }
                break;
            }

            case DaiOpGetLocal: {
                uint8_t slot = READ_BYTE();
                DaiVM_push(vm, frame->slots[slot]);
                break;
            }
            case DaiOpSetLocal: {
                // var 语句和赋值语句都会用到这个指令
                uint8_t slot       = READ_BYTE();
                frame->slots[slot] = DaiVM_pop(vm);
                break;
            }

            case DaiOpGetBuiltin: {
                uint8_t index = READ_BYTE();
                DaiVM_push(vm, vm->builtin_objects[index]);
                break;
            }

            case DaiOpSetFunctionDefault: {
                uint8_t default_count    = READ_BYTE();
                DaiValue value           = DaiVM_peek(vm, default_count);
                DaiObjFunction* function = NULL;
                if (IS_CLOSURE(value)) {
                    function = AS_CLOSURE(value)->function;
                } else {
                    function = AS_FUNCTION(value);
                }
                DaiValue* defaults = VM_ALLOCATE(vm, DaiValue, default_count);
                memcpy(defaults, vm->stack_top - default_count, default_count * sizeof(DaiValue));
                vm->stack_top -= default_count;
                function->defaults      = defaults;
                function->default_count = default_count;
                break;
            }

            case DaiOpClosure: {
                uint16_t function_index = READ_UINT16();
                uint8_t free_var_count  = READ_BYTE();
                DaiValue constant       = chunk->constants.values[function_index];
                DaiValue* frees         = NULL;
                if (free_var_count > 0) {
                    frees = VM_ALLOCATE(vm, DaiValue, free_var_count);
                    for (int i = 0; i < free_var_count; i++) {
                        frees[i] = DaiVM_peek(
                            vm, free_var_count - i - 1);   // peek 里面 -1 ，所以这里要 -1
                    }
                }
                DaiObjClosure* closure = DaiObjClosure_New(vm, AS_FUNCTION(constant));
                closure->frees         = frees;
                vm->stack_top -= free_var_count;
                DaiVM_push(vm, OBJ_VAL(closure));
                break;
            }

            case DaiOpGetFree: {
                uint8_t index = READ_BYTE();
                DaiVM_push(vm, frame->closure->frees[index]);
                break;
            }

            case DaiOpClass: {
                uint16_t name_index = READ_UINT16();
                DaiValue name       = chunk->constants.values[name_index];
                DaiVM_pauseGC(vm);
                DaiObjClass* cls = DaiObjClass_New(vm, AS_STRING(name));
                DaiVM_push(vm, OBJ_VAL(cls));
                DaiVM_resumeGC(vm);
                break;
            }
            case DaiOpDefineField: {
                uint16_t name_index = READ_UINT16();
                uint8_t is_const    = READ_BYTE();
                DaiObjString* name  = AS_STRING(chunk->constants.values[name_index]);
                DaiObjClass* klass  = AS_CLASS(DaiVM_peek(vm, 1));
                DaiValue value      = DaiVM_pop(vm);
                DaiObjClass_define_field(klass, name, value, is_const);
                break;
            }
            case DaiOpDefineMethod: {
                uint16_t name_index = READ_UINT16();
                DaiObjString* name  = AS_STRING(chunk->constants.values[name_index]);
                DaiObjClass* klass  = AS_CLASS(DaiVM_peek(vm, 1));
                DaiValue value      = DaiVM_pop(vm);
                DaiObjClass_define_method(klass, name, value);
                break;
            }
            case DaiOpDefineClassField: {
                uint16_t name_index = READ_UINT16();
                uint8_t is_const    = READ_BYTE();
                DaiObjString* name  = AS_STRING(chunk->constants.values[name_index]);
                DaiObjClass* klass  = AS_CLASS(DaiVM_peek(vm, 1));
                DaiValue value      = DaiVM_pop(vm);
                DaiObjClass_define_class_field(klass, name, value, is_const);
                break;
            }
            case DaiOpDefineClassMethod: {
                uint16_t name_index = READ_UINT16();
                DaiObjString* name  = AS_STRING(chunk->constants.values[name_index]);
                DaiObjClass* klass  = AS_CLASS(DaiVM_peek(vm, 1));
                DaiValue method     = DaiVM_pop(vm);
                DaiObjClass_define_class_method(klass, name, method);
                break;
            }
            case DaiOpGetProperty: {
                uint16_t name_index = READ_UINT16();
                DaiObjString* name  = AS_STRING(chunk->constants.values[name_index]);
                DaiValue receiver   = DaiVM_pop(vm);
                GetPropertyFn func  = NULL;
                if (IS_OBJ(receiver)) {
                    func = AS_OBJ(receiver)->operation->get_property_func;
                }
                if (func) {
                    DaiValue res = func(vm, receiver, name);
                    if (DAI_IS_ERROR(res)) {
                        return AS_ERROR(res);
                    }
                    DaiVM_push(vm, res);
                } else {
                    return DaiObjError_Newf(vm,
                                            "'%s' object has not property '%s'",
                                            dai_value_ts(receiver),
                                            name->chars);
                }
                break;
            }
            case DaiOpSetProperty: {
                uint16_t name_index = READ_UINT16();
                DaiObjString* name  = AS_STRING(chunk->constants.values[name_index]);
                DaiValue receiver   = DaiVM_pop(vm);
                DaiValue value      = DaiVM_pop(vm);
                SetPropertyFn func  = NULL;
                if (IS_OBJ(receiver)) {
                    func = AS_OBJ(receiver)->operation->set_property_func;
                }
                if (func) {
                    DaiValue res = func(vm, receiver, name, value);
                    if (DAI_IS_ERROR(res)) {
                        return AS_ERROR(res);
                    }
                } else {
                    return DaiObjError_Newf(vm,
                                            "'%s' object can not set property '%s'",
                                            dai_value_ts(receiver),
                                            name->chars);
                }
                break;
            }
            case DaiOpGetSelfProperty: {
                uint16_t name_index = READ_UINT16();
                DaiObjString* name  = AS_STRING(chunk->constants.values[name_index]);
                DaiValue receiver   = frame->slots[0];
                DaiValue res = AS_OBJ(receiver)->operation->get_property_func(vm, receiver, name);
                if (DAI_IS_ERROR(res)) {
                    return AS_ERROR(res);
                }
                DaiVM_push(vm, res);
                break;
            }
            case DaiOpSetSelfProperty: {
                uint16_t name_index = READ_UINT16();
                DaiObjString* name  = AS_STRING(chunk->constants.values[name_index]);
                DaiValue receiver   = frame->slots[0];
                DaiValue res        = AS_OBJ(receiver)->operation->set_property_func(
                    vm, receiver, name, DaiVM_pop(vm));
                if (DAI_IS_ERROR(res)) {
                    return AS_ERROR(res);
                }
                break;
            }
            case DaiOpInherit: {
                DaiObjClass* parent = AS_CLASS(DaiVM_pop(vm));
                DaiObjClass* child  = AS_CLASS(DaiVM_peek(vm, 0));
                DaiObjClass_inherit(child, parent);
                break;
            }
            case DaiOpGetSuperProperty: {
                if (frame->function->superclass == NULL) {
                    return DaiObjError_Newf(vm, "no superclass found");
                }
                uint16_t name_index             = READ_UINT16();
                DaiObjString* name              = AS_STRING(chunk->constants.values[name_index]);
                DaiObjBoundMethod* bound_method = DaiObjClass_get_super_method(
                    vm, frame->function->superclass, name, frame->slots[0]);
                if (bound_method == NULL) {
                    return DaiObjError_Newf(
                        vm, "'super' object has not property '%s'", name->chars);
                }
                DaiVM_push(vm, OBJ_VAL(bound_method));
                break;
            }
            case DaiOpCallMethod: {
                uint16_t name_index = READ_UINT16();
                DaiObjString* name  = AS_STRING(chunk->constants.values[name_index]);
                int argCount        = READ_BYTE();
                DaiValue receiver   = DaiVM_peek(vm, argCount);
                GetMethodFn func    = NULL;
                if (IS_OBJ(receiver)) {
                    func = AS_OBJ(receiver)->operation->get_method_func;
                }
                if (func) {
                    DaiValue res = func(vm, receiver, name);
                    if (DAI_IS_ERROR(res)) {
                        return AS_ERROR(res);
                    }
                    DaiObjError* err = DaiVM_callValue(vm, res, argCount, receiver);
                    if (err != NULL) {
                        return err;
                    }
                    frame   = CURRENT_FRAME;
                    chunk   = frame->chunk;
                    globals = frame->globals;
                } else {
                    return DaiObjError_Newf(vm,
                                            "'%s' object has not property '%s'",
                                            dai_value_ts(receiver),
                                            name->chars);
                }
                break;
            }
            case DaiOpCallSelfMethod: {
                uint16_t name_index = READ_UINT16();
                DaiObjString* name  = AS_STRING(chunk->constants.values[name_index]);
                int argCount        = READ_BYTE();
                DaiValue receiver   = frame->slots[0];
                GetMethodFn func    = AS_OBJ(receiver)->operation->get_method_func;
                if (func) {
                    DaiValue res = func(vm, receiver, name);
                    if (DAI_IS_ERROR(res)) {
                        return AS_ERROR(res);
                    }
                    DaiObjError* err = DaiVM_callValue(vm, res, argCount, receiver);
                    if (err != NULL) {
                        return err;
                    }
                    frame   = CURRENT_FRAME;
                    chunk   = frame->chunk;
                    globals = frame->globals;
                } else {
                    return DaiObjError_Newf(vm,
                                            "'%s' object has not property '%s'",
                                            dai_value_ts(receiver),
                                            name->chars);
                }
                break;
            }
            case DaiOpCallSuperMethod: {
                uint16_t name_index = READ_UINT16();
                DaiObjString* name  = AS_STRING(chunk->constants.values[name_index]);
                int argCount        = READ_BYTE();
                DaiValue receiver   = frame->slots[0];
                DaiValue method;
                DaiObj_get_method(vm, frame->function->superclass, receiver, name, &method);
                if (DAI_IS_ERROR(method)) {
                    return AS_ERROR(method);
                }
                DaiObjError* err = DaiVM_callValue(vm, method, argCount, receiver);
                if (err != NULL) {
                    return err;
                }
                frame   = CURRENT_FRAME;
                chunk   = frame->chunk;
                globals = frame->globals;
                break;
            }

            case DaiOpEnd: {
                // 退出 module 调用帧
                // 假装设置模块的返回值（方便测试）
                DaiValue result = *(vm->stack_top);
                vm->frameCount--;
                vm->stack_top    = frame->slots;
                *(vm->stack_top) = result;
                return NULL;
            }

            default: {
                return DaiObjError_Newf(vm,

                                        "Unknown opcode: %s(%d)",
                                        dai_opcode_name(op),
                                        op);
            }
        }
    }
    return NULL;

#undef ARITHMETIC_OPERATION
#undef READ_UINT16
#undef READ_BYTE
}

DaiObjError*
DaiVM_runModule(DaiVM* vm, DaiObjModule* module) {
    vm->state = VMState_running;
    DaiObjMap_cset(vm->modules, OBJ_VAL(module->filename), OBJ_VAL(module));
    if (vm->frameCount == FRAMES_MAX) {
        return DaiObjError_Newf(vm, "maximum recursion depth exceeded");
    }
    CallFrame* frame       = &vm->frames[vm->frameCount++];
    frame->function        = NULL;
    frame->closure         = NULL;
    frame->ip              = module->chunk.code;
    frame->chunk           = &module->chunk;
    frame->slots           = vm->stack_top;
    frame->globals         = module->globals;
    frame->max_local_count = module->max_local_count;
    vm->stack_top          = frame->slots + frame->max_local_count;   // 预分配局部变量空间
    return DaiVM_runCurrentFrame(vm);
}

DaiObjModule*
DaiVM_getModule(DaiVM* vm, const char* filename) {
    DaiObjString* name = dai_copy_string_intern(vm, filename, strlen(filename));
    DaiValue value;
    if (DaiObjMap_cget(vm->modules, OBJ_VAL(name), &value)) {
        return AS_MODULE(value);
    }
    return NULL;
}

DaiObjError*
DaiVM_loadModule(DaiVM* vm, const char* text, DaiObjModule* module) {
    vm->state = VMState_pending;
    DaiTokenList tlist;
    DaiTokenList_init(&tlist);
    DaiAstProgram program;
    DaiAstProgram_init(&program);

    DaiError* err = NULL;
    err           = dai_tokenize_string(text, &tlist);
    if (err != NULL) {
        DaiSyntaxError_setFilename(err, module->filename->chars);
        DaiSyntaxError_pprint(err, text);
        goto DAI_LOAD_MODULE_ERROR;
    }
    err = dai_parse(&tlist, &program);
    if (err != NULL) {
        DaiSyntaxError_setFilename(err, module->filename->chars);
        DaiSyntaxError_pprint(err, text);
        goto DAI_LOAD_MODULE_ERROR;
    }
    err = dai_compile(&program, module, vm);
    if (err != NULL) {
        DaiCompileError_pprint(err, text);
        goto DAI_LOAD_MODULE_ERROR;
    }
    DaiTokenList_reset(&tlist);
    DaiAstProgram_reset(&program);
    DaiObjMap_cset(vm->modules, OBJ_VAL(module->filename), OBJ_VAL(module));
    return DaiVM_runModule(vm, module);

DAI_LOAD_MODULE_ERROR:
    if (err != NULL) {
        DaiError_free(err);
    }
    DaiTokenList_reset(&tlist);
    DaiAstProgram_reset(&program);
    abort();
}

DaiValue
DaiVM_runCall(DaiVM* vm, DaiValue callee, int argCount, ...) {
    DaiVM_push(vm, callee);
    va_list args;
    va_start(args, argCount);
    for (int i = 0; i < argCount; i++) {
        DaiVM_push(vm, va_arg(args, DaiValue));
    }
    va_end(args);
    DaiObjError* err = NULL;
    err              = DaiVM_callValue(vm, callee, argCount, UNDEFINED_VAL);
    if (err != NULL) {
        return OBJ_VAL(err);
    }
    err = DaiVM_runCurrentFrame(vm);
    if (err != NULL) {
        return OBJ_VAL(err);
    }
    return DaiVM_pop(vm);
}

DaiValue
DaiVM_runCall2(DaiVM* vm, DaiValue callee, int argCount) {
    DaiObjError* err = NULL;
    err              = DaiVM_callValue(vm, callee, argCount, UNDEFINED_VAL);
    if (err != NULL) {
        return OBJ_VAL(err);
    }
    err = DaiVM_runCurrentFrame(vm);
    if (err != NULL) {
        return OBJ_VAL(err);
    }
    return DaiVM_pop(vm);
}

void
DaiVM_printError(DaiVM* vm, DaiObjError* err) {
    dai_log("Traceback:\n");
    for (int i = 1; i < vm->frameCount; ++i) {
        CallFrame* frame = &vm->frames[i];
        DaiChunk* chunk  = frame->chunk;
        int lineno       = DaiChunk_getLine(chunk, (int)(frame->ip - chunk->code - 1));
        const char* name = "<module>";
        if (frame->function) {
            // 运行 module 的帧没有 function
            name = DaiObjFunction_name(frame->function);
        }
        dai_log("  File \"%s\", line %d, in %s\n", chunk->filename, lineno, name);
        char* line = dai_line_from_file(chunk->filename, lineno);
        if (line != NULL) {
            // remove prefix space
            char* tmp = line;
            while (tmp[0] == ' ') {
                tmp++;
            }
            dai_log("    %s\n", tmp);
            dai_free(line);
        }
    }
    dai_log("Error: %s\n", err->message);
}

void
DaiVM_printError2(DaiVM* vm, DaiObjError* err, const char* input) {
    dai_log("Traceback:\n");
    for (int i = 1; i < vm->frameCount; ++i) {
        CallFrame* frame = &vm->frames[i];
        DaiChunk* chunk  = frame->chunk;
        int lineno       = DaiChunk_getLine(chunk, (int)(frame->ip - chunk->code));
        const char* name = "<module>";
        if (frame->function) {
            // 运行 module 的帧没有 function
            name = DaiObjFunction_name(frame->function);
        }
        dai_log("  File \"%s\", line %d, in %s\n", chunk->filename, lineno, name);
        char* line = dai_get_line(input, lineno);
        if (line != NULL) {
            // remove prefix space
            char* tmp = line;
            while (tmp[0] == ' ') {
                tmp++;
            }
            dai_log("    %s\n", tmp);
            dai_free(line);
        }
    }
    dai_log("Error: %s\n", err->message);
}

static void
DaiVM_push(DaiVM* vm, DaiValue value) {
    *vm->stack_top = value;
    vm->stack_top++;
    assert(vm->stack_top <= vm->stack_max);
}

static DaiValue
DaiVM_pop(DaiVM* vm) {
    vm->stack_top--;
    return *vm->stack_top;
}

static void
DaiVM_popN(DaiVM* vm, int n) {
    vm->stack_top -= n;
}

void
DaiVM_setTempRef(DaiVM* vm, DaiValue value) {
    vm->temp_ref = value;
}

const char*
DaiVM_getCurrentFilename(const DaiVM* vm) {
    const CallFrame* frame = CURRENT_FRAME;
    assert(frame->chunk != NULL);
    return frame->chunk->filename;
}

void
DaiVM_pauseGC(DaiVM* vm) {
    vm->state = VMState_pending;
}

void
DaiVM_resumeGC(DaiVM* vm) {
    vm->state = VMState_running;
}

void
DaiVM_push1(DaiVM* vm, DaiValue value) {
    DaiVM_push(vm, value);
}

void
DaiVM_getSeed2(DaiVM* vm, uint64_t* seed0, uint64_t* seed1) {
    *seed0 = *(uint64_t*)vm->seed;
    *seed1 = *(uint64_t*)(vm->seed + 8);
}

// #endregion

// #region 用于测试的函数
DaiValue
DaiVM_lastPopedStackElem(const DaiVM* vm) {
    return *vm->stack_top;
}
bool
DaiVM_isEmptyStack(const DaiVM* vm) {
    return vm->stack_top == vm->stack;
}

// #endregion
