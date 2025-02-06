#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dai_chunk.h"
#include "dai_malloc.h"
#include "dai_memory.h"
#include "dai_object.h"
#include "dai_symboltable.h"
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
DaiVM_callValue(DaiVM* vm, const DaiValue callee, const int argCount);

DaiValue dai_true  = {.type = DaiValueType_bool, .as.boolean = true};
DaiValue dai_false = {.type = DaiValueType_bool, .as.boolean = false};

void
DaiVM_resetStack(DaiVM* vm) {
    vm->stack_top  = vm->stack;
    vm->frameCount = 0;
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
    vm->globalSymbolTable = DaiSymbolTable_New();
    vm->globals           = malloc(GLOBAL_MAX * sizeof(DaiValue));
    if (vm->globals == NULL) {
        dai_error("malloc globals error\n");
        abort();
    }
    memset(vm->globals, 0, GLOBAL_MAX * sizeof(DaiValue));

    // 初始化内置函数
    {
        for (int i = 0; i < 256; i++) {
            if (builtin_funcs[i].name == NULL) {
                break;
            }
            DaiSymbolTable_defineBuiltin(vm->globalSymbolTable, i, builtin_funcs[i].name);
            DaiSymbol symbol;
            bool found =
                DaiSymbolTable_resolve(vm->globalSymbolTable, builtin_funcs[i].name, &symbol);
            assert(found);
            vm->builtin_funcs[i] = OBJ_VAL(&builtin_funcs[i]);
        }
    }
}
void
DaiVM_reset(DaiVM* vm) {
    free(vm->globals);
    vm->globals = NULL;
    DaiSymbolTable_free(vm->globalSymbolTable);
    vm->globalSymbolTable = NULL;
    DaiTable_reset(&vm->strings);
    dai_free_objects(vm);
}

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
    if (argCount < function->arity - function->default_count || argCount > function->arity) {
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
    frame->ip        = function->chunk.code;
    // 前面有一个空位用来放 self ，实际是占了被调用函数本身的位置，会在后续操作中更新成 self
    // (例如 DaiVM_callValue DaiObjType_boundMethod 分支)。
    // 因为帧上面有函数的指针，所以占了也没关系
    frame->slots = vm->stack_top - function->arity - 1;
    return NULL;
}

static DaiObjError*
DaiVM_callValue(DaiVM* vm, const DaiValue callee, const int argCount) {
    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
            case DaiObjType_boundMethod: {
                DaiObjBoundMethod* bound_method = AS_BOUND_METHOD(callee);
                DaiObjError* err = DaiVM_callValue(vm, OBJ_VAL(bound_method->method), argCount);
                if (err != NULL) {
                    return err;
                }
                CallFrame* frame = CURRENT_FRAME;
                frame->slots[0]  = bound_method->receiver;
                return NULL;
            }
            case DaiObjType_class: {
                DaiValue* new_stack_top = vm->stack_top - argCount - 1;
                DaiValue result         = DaiObjClass_call(
                    (DaiObjClass*)AS_OBJ(callee), vm, argCount, vm->stack_top - argCount);
                if (vm->stack_top != new_stack_top) {
                    // 如果内部调用了自定义 init 函数，那么栈顶已经更新；否则需要手动更新
                    vm->stack_top = new_stack_top;
                }
                if (IS_ERROR(result)) {
                    return AS_ERROR(result);
                }
                DaiVM_push(vm, result);
                return NULL;
            }
            case DaiObjType_function: {
                return DaiVM_call(vm, (DaiObjFunction*)AS_OBJ(callee), argCount);
            }
            case DaiObjType_builtinFn: {
                const BuiltinFn func = AS_BUILTINFN(callee)->function;
                const DaiValue result =
                    func(vm, DaiVM_peek(vm, argCount), argCount, vm->stack_top - argCount);
                vm->stack_top = vm->stack_top - argCount - 1;
                if (IS_ERROR(result)) {
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

// exitOnReturn 为 true 时，遇到 return 时返回 NULL，否则返回错误
static DaiObjError*
DaiVM_dorun(DaiVM* vm, bool exitOnReturn) {
    CallFrame* frame = &vm->frames[vm->frameCount - 1];
    DaiChunk* chunk  = &frame->function->chunk;

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
    const char* name = NULL;
#endif
    //        while (frame->ip < chunk->code + chunk->count) {
    while (true) {
#ifdef DEBUG_TRACE_EXECUTION
        const char* funcname = DaiObjFunction_name(frame->function);
        if (name != funcname) {
            name = funcname;
            dai_log("========== %s =========\n", name);
        }
        dai_log("          ");
        for (DaiValue* slot = vm->stack; slot < vm->stack_top; slot++) {
            dai_log("[ ");
            dai_print_value(*slot);
            dai_log(" ]");
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
                DaiValue receiver = DaiVM_peek(vm, 1);
                if (IS_OBJ(receiver)) {
                    DaiValue result = AS_OBJ(receiver)->operation->subscript_get_func(
                        vm, receiver, DaiVM_peek(vm, 0));
                    if (IS_ERROR(result)) {
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
                DaiValue receiver = DaiVM_peek(vm, 1);
                if (IS_OBJ(receiver)) {
                    DaiValue result = AS_OBJ(receiver)->operation->subscript_set_func(
                        vm, receiver, DaiVM_peek(vm, 0), DaiVM_peek(vm, 2));
                    vm->stack_top = vm->stack_top - 3;   // 弹出 index, object, value
                    if (IS_ERROR(result)) {
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
            case DaiOpAnd: {
                DaiValue b = DaiVM_pop(vm);
                DaiValue a = DaiVM_pop(vm);
                if (!dai_value_is_truthy(a)) {
                    DaiVM_push(vm, a);
                } else {
                    DaiVM_push(vm, b);
                }
                break;
            }
            case DaiOpOr: {
                DaiValue b = DaiVM_pop(vm);
                DaiValue a = DaiVM_pop(vm);
                if (dai_value_is_truthy(a)) {
                    DaiVM_push(vm, a);
                } else {
                    DaiVM_push(vm, b);
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
                DaiValue val = DaiVM_peek(vm, 0);
                if (dai_value_is_iterable(val)) {
                    DaiValue iterator = AS_OBJ(val)->operation->iter_init_func(vm, val);
                    // 将栈顶的 val 替换成 iterator
                    vm->stack_top[-1] = iterator;
                } else {
                    return DaiObjError_Newf(vm, "'%s' object is not iterable", dai_value_ts(val));
                }
                break;
            }
            case DaiOpIterNext: {
                uint8_t iterator_index = READ_BYTE();
                uint16_t end_offset    = READ_UINT16();
                DaiValue iterator      = frame->slots[iterator_index];
                DaiValue i, e;
                DaiValue next = AS_OBJ(iterator)->operation->iter_next_func(vm, iterator, &i, &e);
                DaiVM_push(vm, i);
                DaiVM_push(vm, e);
                if (IS_UNDEFINED(next)) {
                    frame->ip += end_offset;
                }
                break;
            }

            case DaiOpSetStackTop: {
                uint8_t n     = READ_BYTE();
                vm->stack_top = frame->slots + n;
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
                uint16_t globalIndex     = READ_UINT16();
                DaiValue val             = DaiVM_pop(vm);
                vm->globals[globalIndex] = val;
                break;
            }
            case DaiOpGetGlobal: {
                uint16_t globalIndex = READ_UINT16();
                DaiValue val         = vm->globals[globalIndex];
                DaiVM_push(vm, val);
                break;
            }

            case DaiOpCall: {
                int argCount     = READ_BYTE();
                DaiValue callee  = DaiVM_peek(vm, argCount);
                DaiObjError* err = DaiVM_callValue(vm, callee, argCount);
                if (err != NULL) {
                    return err;
                }
                frame = CURRENT_FRAME;
                chunk = &frame->function->chunk;
                break;
            }
            case DaiOpReturnValue: {
                DaiValue result = DaiVM_pop(vm);
                vm->frameCount--;
                vm->stack_top = frame->slots;
                DaiVM_push(vm, result);
                frame = &vm->frames[vm->frameCount - 1];
                chunk = &frame->function->chunk;
                if (exitOnReturn) {
                    return NULL;
                }
                break;
            }
            case DaiOpReturn: {
                DaiValue result = NIL_VAL;
                vm->frameCount--;
                vm->stack_top = frame->slots;
                DaiVM_push(vm, result);
                frame = &vm->frames[vm->frameCount - 1];
                chunk = &frame->function->chunk;
                if (exitOnReturn) {
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
                uint8_t slot = READ_BYTE();
                if (frame->slots + slot < vm->stack_top - 1) {
                    // 如果是赋值语句，那么需要先弹出栈顶的值
                    frame->slots[slot] = DaiVM_pop(vm);
                }
                // 如果是 var 语句，那么什么都不用做，值已经在对应的 slot 里面了
                break;
            }

            case DaiOpGetBuiltin: {
                uint8_t index = READ_BYTE();
                DaiVM_push(vm, vm->builtin_funcs[index]);
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
                DaiObjClass* cls    = DaiObjClass_New(vm, AS_STRING(name));
                DaiVM_push(vm, OBJ_VAL(cls));
                break;
            }
            case DaiOpDefineField: {
                uint16_t name_index = READ_UINT16();
                DaiObjString* name  = AS_STRING(chunk->constants.values[name_index]);
                DaiObjClass* cls    = AS_CLASS(DaiVM_peek(vm, 1));
                if (!DaiTable_has(&cls->fields, name)) {
                    DaiValueArray_write(&cls->field_names, OBJ_VAL(name));
                }
                DaiTable_set(&cls->fields, name, DaiVM_pop(vm));
                break;
            }
            case DaiOpDefineMethod: {
                uint16_t name_index = READ_UINT16();
                DaiObjString* name  = AS_STRING(chunk->constants.values[name_index]);
                DaiObjClass* klass  = AS_CLASS(DaiVM_peek(vm, 1));
                DaiValue value      = DaiVM_pop(vm);
                // 设置方法的 super class
                {
                    DaiObjFunction* function = NULL;
                    if (IS_CLOSURE(value)) {
                        function = AS_CLOSURE(value)->function;
                    } else {
                        function = AS_FUNCTION(value);
                    }
                    function->superclass = klass->parent;
                }
                DaiTable_set(&klass->methods, name, value);
                if (strcmp(name->chars, "init") == 0) {
                    klass->init = value;
                }
                break;
            }
            case DaiOpDefineClassField: {
                uint16_t name_index = READ_UINT16();
                DaiObjString* name  = AS_STRING(chunk->constants.values[name_index]);
                DaiObjClass* klass  = AS_CLASS(DaiVM_peek(vm, 1));
                DaiValue value      = DaiVM_pop(vm);
                DaiTable_set(&klass->class_fields, name, value);
                break;
            }
            case DaiOpDefineClassMethod: {
                uint16_t name_index = READ_UINT16();
                DaiObjString* name  = AS_STRING(chunk->constants.values[name_index]);
                DaiObjClass* klass  = AS_CLASS(DaiVM_peek(vm, 1));
                DaiValue method     = DaiVM_pop(vm);
                // 设置方法的 super class
                {
                    DaiObjFunction* function = NULL;
                    if (IS_CLOSURE(method)) {
                        function = AS_CLOSURE(method)->function;
                    } else {
                        function = AS_FUNCTION(method);
                    }
                    function->superclass = klass->parent;
                }
                DaiTable_set(&klass->class_methods, name, method);
                break;
            }
            case DaiOpGetProperty: {
                uint16_t name_index = READ_UINT16();
                DaiObjString* name  = AS_STRING(chunk->constants.values[name_index]);
                DaiValue receiver   = DaiVM_pop(vm);
                if (IS_OBJ(receiver)) {
                    DaiValue res =
                        AS_OBJ(receiver)->operation->get_property_func(vm, receiver, name);
                    if (IS_ERROR(res)) {
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
                if (IS_OBJ(receiver)) {
                    DaiValue res =
                        AS_OBJ(receiver)->operation->set_property_func(vm, receiver, name, value);
                    if (IS_ERROR(res)) {
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
                if (IS_ERROR(res)) {
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
                if (IS_ERROR(res)) {
                    return AS_ERROR(res);
                }
                break;
            }
            case DaiOpInherit: {
                DaiObjClass* parent = AS_CLASS(DaiVM_pop(vm));
                DaiObjClass* child  = AS_CLASS(DaiVM_peek(vm, 0));
                child->parent       = parent;
                child->init         = parent->init;
                DaiTable_copy(&parent->fields, &child->fields);
                DaiValueArray_copy(&parent->field_names, &child->field_names);
                DaiTable_copy(&parent->class_fields, &child->class_fields);
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
                if (IS_OBJ(receiver)) {
                    DaiValue res =
                        AS_OBJ(receiver)->operation->get_property_func(vm, receiver, name);
                    if (IS_ERROR(res)) {
                        return AS_ERROR(res);
                    }
                    DaiObjError* err = DaiVM_callValue(vm, res, argCount);
                    if (err != NULL) {
                        return err;
                    }
                    // 如果是内置函数，那么不需要更新 frame 和 chunk
                    if (!IS_BUILTINFN(res)) {
                        frame           = CURRENT_FRAME;
                        frame->slots[0] = receiver;
                        chunk           = &frame->function->chunk;
                    }
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
                if (IS_OBJ(receiver)) {
                    DaiValue res =
                        AS_OBJ(receiver)->operation->get_property_func(vm, receiver, name);
                    if (IS_ERROR(res)) {
                        return AS_ERROR(res);
                    }
                    DaiObjError* err = DaiVM_callValue(vm, res, argCount);
                    if (err != NULL) {
                        return err;
                    }
                    frame           = CURRENT_FRAME;
                    frame->slots[0] = receiver;
                    chunk           = &frame->function->chunk;
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
                if (IS_ERROR(method)) {
                    return AS_ERROR(method);
                }
                DaiObjError* err = DaiVM_callValue(vm, method, argCount);
                if (err != NULL) {
                    return err;
                }
                frame           = CURRENT_FRAME;
                frame->slots[0] = receiver;
                chunk           = &frame->function->chunk;
                break;
            }

            case DaiOpEnd: return NULL;

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
DaiVM_run(DaiVM* vm, DaiObjFunction* function) {
    vm->state = VMState_running;
    // 把函数压入栈，然后调用他
    DaiVM_push(vm, OBJ_VAL(function));
    DaiObjError* err = DaiVM_call(vm, function, 0);
    if (err != NULL) {
        return err;
    }
    // 弹出脚本函数，这样初始运行时整个栈都是空的
    DaiVM_pop(vm);
    return DaiVM_dorun(vm, false);
}

void
DaiVM_push1(DaiVM* vm, DaiValue value) {
    DaiVM_push(vm, value);
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
    err              = DaiVM_callValue(vm, callee, argCount);
    if (err != NULL) {
        return OBJ_VAL(err);
    }
    err = DaiVM_dorun(vm, true);
    if (err != NULL) {
        return OBJ_VAL(err);
    }
    return DaiVM_pop(vm);
}
DaiValue
DaiVM_runCall2(DaiVM* vm, DaiValue callee, int argCount) {
    DaiObjError* err = NULL;
    err              = DaiVM_callValue(vm, callee, argCount);
    if (err != NULL) {
        return OBJ_VAL(err);
    }
    err = DaiVM_dorun(vm, true);
    if (err != NULL) {
        return OBJ_VAL(err);
    }
    return DaiVM_pop(vm);
}
DaiValue
DaiVM_runCall3(DaiVM* vm, DaiValue callee, DaiValueArray* args) {
    DaiVM_push(vm, callee);
    for (int i = 0; i < args->count; i++) {
        DaiVM_push(vm, args->values[i]);
    }
    DaiObjError* err = NULL;
    err              = DaiVM_callValue(vm, callee, args->count);
    if (err != NULL) {
        return OBJ_VAL(err);
    }
    err = DaiVM_dorun(vm, true);
    if (err != NULL) {
        return OBJ_VAL(err);
    }
    return DaiVM_pop(vm);
}

void
DaiVM_printError(DaiVM* vm, DaiObjError* err) {
    dai_log("Traceback:\n");
    for (int i = 0; i < vm->frameCount; ++i) {
        CallFrame* frame         = &vm->frames[i];
        DaiChunk* chunk          = &frame->function->chunk;
        int lineno               = DaiChunk_getLine(chunk, (int)(frame->ip - chunk->code));
        DaiObjFunction* function = frame->function;
        dai_log("  File \"%s\", line %d, in %s\n",
                chunk->filename,
                lineno,
                DaiObjFunction_name(function));
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
    for (int i = 0; i < vm->frameCount; ++i) {
        CallFrame* frame         = &vm->frames[i];
        DaiChunk* chunk          = &frame->function->chunk;
        int lineno               = DaiChunk_getLine(chunk, (int)(frame->ip - chunk->code));
        DaiObjFunction* function = frame->function;
        dai_log("  File \"%s\", line %d, in %s\n",
                chunk->filename,
                lineno,
                DaiObjFunction_name(function));
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
DaiValue
DaiVM_stackTop(const DaiVM* vm) {
    return vm->stack_top[-1];
}

void
DaiVM_setTempRef(DaiVM* vm, DaiValue value) {
    vm->temp_ref = value;
}

bool
DaiVM_getGlobal(DaiVM* vm, const char* name, DaiValue* value) {
    DaiSymbol symbol;
    if (DaiSymbolTable_resolve(vm->globalSymbolTable, name, &symbol)) {
        *value = vm->globals[symbol.index];
        return true;
    }
    return false;
}

bool
DaiVM_setGlobal(DaiVM* vm, const char* name, DaiValue value) {
    DaiSymbol symbol;
    if (DaiSymbolTable_resolve(vm->globalSymbolTable, name, &symbol)) {
        vm->globals[symbol.index] = value;
        return true;
    }
    return false;
}

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
