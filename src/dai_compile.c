#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "dai_array.h"
#include "dai_ast/dai_astbase.h"
#include "dai_ast/dai_astexpression.h"
#include "dai_ast/dai_asttype.h"
#include "dai_chunk.h"
#include "dai_common.h"
#include "dai_compile.h"
#include "dai_error.h"
#include "dai_memory.h"
#include "dai_object.h"
#include "dai_symboltable.h"

#ifndef MAX
#    define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

// #region IntArray 记录 break continue 层级和位置，辅助编译跳转指令
typedef struct {
    int level;
    int value;
} LevelInt;

typedef struct {
    int capacity;
    int count;
    int level;
    LevelInt* values;
} IntArray;
void
IntArray_init(IntArray* array) {
    array->capacity = 0;
    array->count    = 0;
    array->level    = 0;
    array->values   = NULL;
}

void
IntArray_reset(IntArray* array) {
    FREE_ARRAY(LevelInt, array->values, array->capacity);
    IntArray_init(array);
}

void
IntArray_push(IntArray* array, int value) {
    if (array->capacity < array->count + 1) {
        int oldCapacity = array->capacity;
        array->capacity += 8;
        array->values = GROW_ARRAY(LevelInt, array->values, oldCapacity, array->capacity);
    }
    array->values[array->count] = (LevelInt){.value = value, .level = array->level};
    array->count++;
}

LevelInt
IntArray_pop(IntArray* array) {
    array->count--;
    return array->values[array->count];
}

// -1 表示没有了
int
IntArray_popCurrentLevel(IntArray* array) {
    if (array->count == 0) {
        return -1;
    }
    LevelInt v = array->values[array->count - 1];
    if (v.level == array->level) {
        array->count--;
        return v.value;
    }
    return -1;
}

void
IntArray_enter(IntArray* array) {
    array->level++;
}

void
IntArray_leave(IntArray* array) {
    array->level--;
}

bool
IntArray_contains(const IntArray* array, const int value) {
    for (int i = 0; i < array->count; i++) {
        if (array->values[i].value == value) {
            return true;
        }
    }
    return false;
}

void
IntArray_show(const IntArray* array) {
    dai_log("IntArray level=%d, count=%d [\n", array->level, array->count);
    for (int i = 0; i < array->count; i++) {
        dai_log("    level=%d, value=%d\n", array->values[i].level, array->values[i].value);
    }
    dai_log("]\n");
}

// #endregion


// #region DaiCompiler

#define JUMP_PLACEHOLDER UINT16_MAX

typedef enum {
    FunctionType_classMethod,
    FunctionType_method,
    FunctionType_function,
    FunctionType_script,
} FunctionType;

typedef enum {
    ScopeType_script,
    ScopeType_function,
    ScopeType_class,
    ScopeType_method,
    ScopeType_if,
    ScopeType_while,
    ScopeType_for,
} ScopeType;

typedef struct {
    // filename 不归 DaiCompiler 所有，仅作为编译器的一部分
    const char* filename;
    // chunk 不归 DaiCompiler 所有，仅作为编译器的一部分
    // DaiChunk *chunk;
    // symbolTable 不归 DaiCompiler 所有，仅作为编译器的一部分
    DaiSymbolTable* symbolTable;
    // vm 不归 DaiCompiler 所有，仅作为编译器的一部分
    DaiVM* vm;
    // module 不归 DaiCompiler 所有，仅作为编译器的一部分
    DaiObjModule* module;
    // chunk 不归 DaiCompiler 所有，仅作为编译器的一部分
    DaiChunk* chunk;

    // 函数类型，判断是在解析脚本还是函数
    // 一部分语句（比如 return）只能在函数中使用，不能在脚本中使用
    // 用这个辅助判断
    FunctionType type;

    // 用来判断 block 语句是不是函数块
    bool is_function_block;

    // 记录解析类时的属性名，用来判断重复定义
    DaiTable propertys;

    // 匿名函数数量，用来给匿名函数命名区分
    int anonymous_count;

    IntArray
        scope_stack;   // 记录作用域，用来判断一些语句的合法性，比如 break continue 必须在循环中
    IntArray break_array;      // 记录 break 跳转指令的位置，用于后续更新跳转偏移量
    IntArray continue_array;   // 记录 continue 跳转指令的位置，用于后续更新跳转偏移量

    int max_local_count;   // 记录最大的局部变量数量，用于分配栈空间

} DaiCompiler;

#ifdef DISASSEMBLE_VARIABLE_NAME
void
DaiCompiler_addName(const DaiCompiler* compiler, const char* name, int back) {
    DaiChunk* chunk = compiler->chunk;
    DaiChunk_addName(chunk, name, back);
}
#    define ADD_GLOBAL_NAME(compiler, name) DaiCompiler_addName((compiler), (name), 3)
#    define ADD_LOCAL_NAME(compiler, name) DaiCompiler_addName((compiler), (name), 2)
#    define ADD_LOCAL_NAME3(compiler, name) DaiCompiler_addName((compiler), (name), 3)
#    define ADD_FREE_NAME(compiler, name) DaiCompiler_addName((compiler), (name), 2)

#else
#    define ADD_GLOBAL_NAME(compiler, name) \
        do {                                \
        } while (0)
#    define ADD_LOCAL_NAME(compiler, name) \
        do {                               \
        } while (0)
#    define ADD_LOCAL_NAME3(compiler, name) \
        do {                                \
        } while (0)
#    define ADD_FREE_NAME(compiler, name) \
        do {                              \
        } while (0)

#endif

static int
DaiCompiler_addConstant(const DaiCompiler* compiler, DaiValue value);
static int
DaiCompiler_emit(const DaiCompiler* compiler, DaiOpCode op, int line);
static int
DaiCompiler_emit1(const DaiCompiler* compiler, DaiOpCode op, uint8_t operand, int line);
static int
DaiCompiler_emit2(const DaiCompiler* compiler, DaiOpCode op, uint16_t operand, int line);
static int
DaiCompiler_emit3(const DaiCompiler* compiler, DaiOpCode op, uint16_t operand1, uint8_t operand2,
                  int line);
static int
DaiCompiler_emitIterNext(const DaiCompiler* compiler, uint8_t operand1, int line);
static void
DaiCompiler_patchJump(const DaiCompiler* compiler, int offset);
static void
DaiCompiler_patchJumpBack(const DaiCompiler* compiler, int offset, int dst);
static void
DaiCompiler_reset(DaiCompiler* compiler);
static DaiCompileError*
DaiCompiler_compile(DaiCompiler* compiler, DaiAstBase* node);

static DaiCompileError*
DaiCompiler_loadSymbol(const DaiCompiler* compiler, const DaiSymbol* symbol, int start_line) {
    switch (symbol->type) {
        case DaiSymbolType_builtin: {
            DaiCompiler_emit1(compiler, DaiOpGetBuiltin, symbol->index, start_line);
            break;
        }
        case DaiSymbolType_global: {
            DaiCompiler_emit2(compiler, DaiOpGetGlobal, symbol->index, start_line);
            ADD_GLOBAL_NAME(compiler, symbol->name);
            break;
        }
        case DaiSymbolType_local: {
            DaiCompiler_emit1(compiler, DaiOpGetLocal, symbol->index, start_line);
            ADD_LOCAL_NAME(compiler, symbol->name);
            break;
        }
        case DaiSymbolType_free: {
            DaiCompiler_emit1(compiler, DaiOpGetFree, symbol->index, start_line);
            ADD_FREE_NAME(compiler, symbol->name);
            break;
        }
        case DaiSymbolType_self: {
            if (compiler->type != FunctionType_method &&
                compiler->type != FunctionType_classMethod) {
                return DaiCompileError_New(
                    "Cannot use 'self' outside of a method", compiler->filename, start_line, 0);
            }
            // self 固定在第 0 个
            DaiCompiler_emit1(compiler, DaiOpGetLocal, 0, start_line);
            break;
        }
        default: {
            dai_error("Unknown symbol type: %d\n", symbol->type);
            unreachable();
        }
    }
    return NULL;
}

static void
DaiCompiler_init(DaiCompiler* compiler, DaiObjModule* module, DaiChunk* chunk,
                 DaiSymbolTable* symbolTable, FunctionType type, DaiVM* vm) {
    compiler->filename          = chunk->filename;
    compiler->module            = module;
    compiler->chunk             = chunk;
    compiler->type              = type;
    compiler->vm                = vm;
    compiler->symbolTable       = symbolTable;
    compiler->is_function_block = false;
    DaiTable_init(&compiler->propertys);
    compiler->anonymous_count = 0;
    IntArray_init(&compiler->scope_stack);
    IntArray_init(&compiler->break_array);
    IntArray_init(&compiler->continue_array);
    compiler->max_local_count = 0;
}

static void
DaiCompiler_reset(DaiCompiler* compiler) {
    DaiTable_reset(&compiler->propertys);
    IntArray_reset(&compiler->scope_stack);
    IntArray_reset(&compiler->break_array);
    IntArray_reset(&compiler->continue_array);
}

// 更新符号表
static DaiCompileError*
DaiCompiler_extractSymbol(DaiCompiler* compiler, DaiAstBase* node) {
    char* name = NULL;
    int line   = 0;
    int column = 0;
    switch (node->type) {
        case DaiAstType_program: {
            DaiAstProgram* program = (DaiAstProgram*)node;
            for (int i = 0; i < program->length; i++) {
                DaiCompileError* err =
                    DaiCompiler_extractSymbol(compiler, (DaiAstBase*)program->statements[i]);
                if (err != NULL) {
                    return err;
                }
            }
            break;
        }
        case DaiAstType_VarStatement: {
            DaiAstVarStatement* stmt = (DaiAstVarStatement*)node;
            name                     = stmt->name->value;
            line                     = stmt->name->start_line;
            column                   = stmt->name->start_column;
            break;
        }
        case DaiAstType_FunctionStatement: {
            DaiAstFunctionStatement* stmt = (DaiAstFunctionStatement*)node;
            name                          = stmt->name;
            line                          = stmt->start_line;
            column                        = stmt->start_column;
            break;
        }
        case DaiAstType_ClassStatement: {
            DaiAstClassStatement* stmt = (DaiAstClassStatement*)node;
            name                       = stmt->name;
            line                       = stmt->start_line;
            column                     = stmt->start_column;
            break;
        }
        default: break;
    }
    // 检查全局变量是否重复定义和数量超出限制
    if (name != NULL) {
        DaiSymbol symbol;
        if (DaiSymbolTable_resolve(compiler->symbolTable, name, &symbol)) {
            return DaiCompileError_Newf(
                compiler->filename, line, column, "symbol '%s' already defined", name);
        }
        symbol = DaiSymbolTable_predefine(compiler->symbolTable, name);
        if (symbol.index >= GLOBAL_MAX) {
            return DaiCompileError_Newf(
                compiler->filename, line, column, "too many global symbols");
        }
    }
    return NULL;
}

// #region 编译 AST 节点

static void
DaiCompiler_enterLoop(DaiCompiler* compiler) {
    IntArray_enter(&compiler->break_array);
    IntArray_enter(&compiler->continue_array);
}

static void
DaiCompiler_leaveLoop(DaiCompiler* compiler, int loop_start) {
    // 更新 break 跳转指令的偏移量
    {
        int count = compiler->break_array.count;
        for (int i = 0; i < count; i++) {
            int jump = IntArray_popCurrentLevel(&compiler->break_array);
            if (jump == -1) {
                break;
            }
            DaiCompiler_patchJump(compiler, jump);
        }
    }
    // 更新 continue 跳转指令的偏移量
    {
        int count = compiler->continue_array.count;
        for (int i = 0; i < count; i++) {
            int jump = IntArray_popCurrentLevel(&compiler->continue_array);
            if (jump == -1) {
                break;
            }
            DaiCompiler_patchJumpBack(compiler, jump, loop_start);
        }
    }
    IntArray_leave(&compiler->break_array);
    IntArray_leave(&compiler->continue_array);
}

// 计算执行字节码需要的最大栈空间
static int
calculate_max_stack_size(DaiChunk* chunk) {
    int max_stack_size = 0;
    int stack_size     = 0;
    for (int offset = 0; offset < chunk->count;) {
        const DaiOpCode instruction         = chunk->code[offset];
        const DaiOpCodeDefinition* code_def = dai_opcode_lookup(instruction);
        int stack_size_change               = code_def->stack_size_change;
        if (stack_size_change == STACK_SIZE_CHANGE_DEPENDS_ON_OPERAND) {
            switch (instruction) {
                case DaiOpArray: {
                    uint16_t count    = DaiChunk_readu16(chunk, offset + 1);
                    stack_size_change = -count;
                    break;
                }
                case DaiOpMap: {
                    uint16_t count    = DaiChunk_readu16(chunk, offset + 1);
                    stack_size_change = -(2 * count);
                    break;
                }
                case DaiOpPopN: {
                    uint8_t count     = DaiChunk_read(chunk, offset + 1);
                    stack_size_change = -count;
                    break;
                }
                case DaiOpCall: {
                    uint8_t count = DaiChunk_read(chunk, offset + 1);
                    // 算上调用的函数本身，会弹栈 count + 1
                    stack_size_change = -count - 1;
                    break;
                }
                case DaiOpSetFunctionDefault: {
                    uint8_t count     = DaiChunk_read(chunk, offset + 1);
                    stack_size_change = -count;
                    break;
                }
                case DaiOpClosure: {
                    uint8_t count = DaiChunk_read(chunk, offset + 3);
                    // 弹出 count 个自由变量，压入一个 closure 对象
                    stack_size_change = -count + 1;
                    break;
                }
                case DaiOpCallMethod:
                case DaiOpCallSelfMethod:
                case DaiOpCallSuperMethod: {
                    uint8_t count = DaiChunk_read(chunk, offset + 3);
                    // 算上调用的实例本身，会弹栈 count + 1
                    stack_size_change = -count - 1;
                    break;
                }
                default: {
                    dai_error("unexpected op %s\n", code_def->name);
                    unreachable();
                }
            }
        }
        offset += (code_def->operand_bytes + 1);
        if (stack_size_change < 0) {
            max_stack_size = MAX(max_stack_size, stack_size);
        }
        stack_size = stack_size + stack_size_change;
    }
    max_stack_size = MAX(max_stack_size, stack_size);
    return max_stack_size;
}

static DaiCompileError*
DaiCompiler_compileFunction(DaiCompiler* compiler, DaiAstBase* node) {
    const char* name;   // 函数名字
    FunctionType function_type    = FunctionType_function;
    DaiAstBlockStatement* body    = NULL;
    int parameters_count          = 0;
    DaiAstIdentifier** parameters = NULL;
    int start_line                = 0;
    int end_line                  = 0;
    DaiArray* defaults            = NULL;
    char buf[32];
    switch (node->type) {
        case DaiAstType_FunctionLiteral: {
            body = ((DaiAstFunctionLiteral*)node)->body;
            snprintf(buf, sizeof(buf), "<anonymous %d>", compiler->anonymous_count);
            name             = buf;
            parameters_count = ((DaiAstFunctionLiteral*)node)->parameters_count;
            parameters       = ((DaiAstFunctionLiteral*)node)->parameters;
            start_line       = ((DaiAstFunctionLiteral*)node)->start_line;
            end_line         = ((DaiAstFunctionLiteral*)node)->end_line;
            defaults         = ((DaiAstFunctionLiteral*)node)->defaults;
            compiler->anonymous_count++;
            break;
        }
        case DaiAstType_FunctionStatement: {
            body             = ((DaiAstFunctionStatement*)node)->body;
            name             = ((DaiAstFunctionStatement*)node)->name;
            parameters_count = ((DaiAstFunctionStatement*)node)->parameters_count;
            parameters       = ((DaiAstFunctionStatement*)node)->parameters;
            start_line       = ((DaiAstFunctionStatement*)node)->start_line;
            end_line         = ((DaiAstFunctionStatement*)node)->end_line;
            defaults         = ((DaiAstFunctionStatement*)node)->defaults;
            break;
        }
        case DaiAstType_MethodStatement: {
            body             = ((DaiAstMethodStatement*)node)->body;
            name             = ((DaiAstMethodStatement*)node)->name;
            parameters_count = ((DaiAstMethodStatement*)node)->parameters_count;
            parameters       = ((DaiAstMethodStatement*)node)->parameters;
            start_line       = ((DaiAstMethodStatement*)node)->start_line;
            end_line         = ((DaiAstMethodStatement*)node)->end_line;
            function_type    = FunctionType_method;
            defaults         = ((DaiAstMethodStatement*)node)->defaults;
            break;
        }
        case DaiAstType_ClassMethodStatement: {
            body             = ((DaiAstClassMethodStatement*)node)->body;
            name             = ((DaiAstClassMethodStatement*)node)->name;
            parameters_count = ((DaiAstClassMethodStatement*)node)->parameters_count;
            parameters       = ((DaiAstClassMethodStatement*)node)->parameters;
            start_line       = ((DaiAstClassMethodStatement*)node)->start_line;
            end_line         = ((DaiAstClassMethodStatement*)node)->end_line;
            function_type    = FunctionType_classMethod;
            defaults         = ((DaiAstClassMethodStatement*)node)->defaults;
            break;
        }
        default: {
            dai_error("Unknown function type: %d\n", node->type);
            unreachable();
        }
    }

    // 创建函数对象
    DaiObjFunction* function =
        DaiObjFunction_New(compiler->vm, compiler->module, name, compiler->filename);
    // 创建函数符号表
    DaiSymbolTable* functable = DaiSymbolTable_NewFunction(compiler->symbolTable);
    // 创建子编译器
    DaiCompiler subcompiler;
    DaiCompiler_init(&subcompiler,
                     compiler->module,
                     &function->chunk,
                     functable,
                     FunctionType_function,
                     compiler->vm);
    IntArray_push(&subcompiler.scope_stack, ScopeType_function);
    subcompiler.is_function_block = true;
    subcompiler.anonymous_count   = compiler->anonymous_count;
    subcompiler.type              = function_type;

    // 定义 self
    // 固定将 local=0 的位置设置为 self
    // 所有函数统一定义一个 self ，但是 self 只有方法和类方法里面可以用到
    DaiSymbolTable_defineSelf(functable);
    function->arity = parameters_count;
    // 定义参数
    for (int i = 0; i < parameters_count; i++) {
        DaiAstIdentifier* param = parameters[i];
        DaiSymbolTable_define(functable, param->value, false);
    }
    subcompiler.max_local_count =
        MAX(subcompiler.max_local_count, parameters_count + 1);   // +1 是 self
    // 编译函数体
    DaiCompileError* err = DaiCompiler_compile(&subcompiler, (DaiAstBase*)body);
    if (err != NULL) {
        return err;
    }
    // 给函数末尾统一加一个 return nil 指令，防止函数没有 return
    DaiCompiler_emit(&subcompiler, DaiOpReturn, end_line);
    // 处理函数（函数闭包）的自由变量
    int num_free = 0;
    {
        DaiSymbol* free_symbols = DaiSymbolTable_getFreeSymbols(functable, &num_free);
        for (int i = 0; i < num_free; i++) {
            DaiCompileError* err2 = DaiCompiler_loadSymbol(compiler, &free_symbols[i], start_line);
            if (err2 != NULL) {
                return err2;
            }
        }
    }
    DaiSymbolTable_free(functable);
    DaiCompiler_reset(&subcompiler);

    if (num_free > 0) {
        DaiCompiler_emit3(compiler,
                          DaiOpClosure,
                          DaiCompiler_addConstant(compiler, OBJ_VAL(function)),
                          num_free,
                          start_line);
    } else {
        DaiCompiler_emit2(compiler,
                          DaiOpConstant,
                          DaiCompiler_addConstant(compiler, OBJ_VAL(function)),
                          start_line);
    }


    // 处理函数默认参数
    size_t default_count = DaiArray_length(defaults);
    if (default_count > 0) {
        for (int i = 0; i < default_count; i++) {
            DaiAstExpression* expr = *(DaiAstExpression**)DaiArray_get(defaults, i);
            DaiCompileError* err2  = DaiCompiler_compile(compiler, (DaiAstBase*)expr);
            if (err2 != NULL) {
                return err2;
            }
        }
        DaiCompiler_emit1(compiler, DaiOpSetFunctionDefault, DaiArray_length(defaults), start_line);
    }

    function->max_local_count = subcompiler.max_local_count;
    // 局部变量是预先分配在栈上的，同样需要占用栈空间
    function->max_stack_size =
        function->max_local_count + calculate_max_stack_size(&function->chunk);

    compiler->anonymous_count = subcompiler.anonymous_count;
    return NULL;
}

// 编译 and or 短路运算表达式
static DaiCompileError*
DaiCompiler_compileAndOrExpr(DaiCompiler* compiler, DaiAstInfixExpression* expr) {
    DaiCompileError* err = DaiCompiler_compile(compiler, (DaiAstBase*)expr->left);
    if (err != NULL) {
        return err;
    }
    // 先发出虚假偏移量的跳转指令
    DaiOpCode op = DaiOpAndJump;
    if (strcmp(expr->operator, "or") == 0) {
        op = DaiOpOrJump;
    }
    int jump_offset = DaiCompiler_emit2(compiler, op, JUMP_PLACEHOLDER, expr->start_line);
    err             = DaiCompiler_compile(compiler, (DaiAstBase*)expr->right);
    if (err != NULL) {
        return err;
    }
    DaiCompiler_patchJump(compiler, jump_offset);
    return NULL;
}

static DaiCompileError*
DaiCompiler_compileProgram(DaiCompiler* compiler, DaiAstBase* node) {
    DaiAstProgram* program = (DaiAstProgram*)node;
    for (int i = 0; i < program->length; i++) {
        DaiCompileError* err = DaiCompiler_compile(compiler, (DaiAstBase*)program->statements[i]);
        if (err != NULL) {
            return err;
        }
    }
    return NULL;
}

static DaiCompileError*
DaiCompiler_compileVarStatement(DaiCompiler* compiler, DaiAstBase* node) {
    DaiAstVarStatement* stmt = (DaiAstVarStatement*)node;
    DaiCompileError* err     = DaiCompiler_compile(compiler, (DaiAstBase*)stmt->value);
    if (err != NULL) {
        return err;
    }
    if (DaiSymbolTable_isDefined(compiler->symbolTable, stmt->name->value)) {
        return DaiCompileError_Newf(compiler->filename,
                                    stmt->name->start_line,
                                    stmt->name->start_column,
                                    "symbol '%s' already defined",
                                    stmt->name->value);
    }
    DaiSymbol symbol =
        DaiSymbolTable_define(compiler->symbolTable, stmt->name->value, stmt->is_con);
    switch (symbol.type) {
        case DaiSymbolType_global: {
            DaiCompiler_emit2(compiler, DaiOpDefineGlobal, symbol.index, stmt->start_line);
            ADD_GLOBAL_NAME(compiler, symbol.name);
            break;
        }
        case DaiSymbolType_local: {
            if (symbol.index >= LOCAL_MAX) {
                return DaiCompileError_Newf(compiler->filename,
                                            stmt->name->start_line,
                                            stmt->name->start_column,
                                            "error1 too many local symbols");
            }
            compiler->max_local_count = MAX(compiler->max_local_count, symbol.index + 1);
            DaiCompiler_emit1(compiler, DaiOpSetLocal, symbol.index, stmt->start_line);
            ADD_LOCAL_NAME(compiler, symbol.name);
            break;
        }
        default: {
            unreachable();
        }
    }
    return NULL;
}
static DaiCompileError*
DaiCompiler_compileReturnStatement(DaiCompiler* compiler, DaiAstBase* node) {
    DaiAstReturnStatement* stmt = (DaiAstReturnStatement*)node;
    if (compiler->type == FunctionType_script) {
        return DaiCompileError_New("Can't return from top-level code",
                                   compiler->filename,
                                   stmt->start_line,
                                   stmt->start_column);
    }
    if (stmt->return_value == NULL) {
        DaiCompiler_emit(compiler, DaiOpReturn, stmt->start_line);
    } else {
        DaiCompileError* err = DaiCompiler_compile(compiler, (DaiAstBase*)stmt->return_value);
        if (err != NULL) {
            return err;
        }
        DaiCompiler_emit(compiler, DaiOpReturnValue, stmt->start_line);
    }
    return NULL;
}
static DaiCompileError*
DaiCompiler_compileExpressionStatement(DaiCompiler* compiler, DaiAstBase* node) {
    DaiAstExpressionStatement* stmt = (DaiAstExpressionStatement*)node;
    DaiCompileError* err            = DaiCompiler_compile(compiler, (DaiAstBase*)stmt->expression);
    if (err != NULL) {
        return err;
    }
    DaiCompiler_emit(compiler, DaiOpPop, stmt->start_line);
    return NULL;
}
static DaiCompileError*
DaiCompiler_compileIfStatement(DaiCompiler* compiler, DaiAstBase* node) {
    // 指令结构如下
    // condition
    // jump_if_false @else_branch
    // @then_branch
    // jump @end
    // @else_branch
    // @end
    IntArray_push(&compiler->scope_stack, ScopeType_if);
    DaiAstIfStatement* stmt = (DaiAstIfStatement*)node;
    DaiCompileError* err    = DaiCompiler_compile(compiler, (DaiAstBase*)stmt->condition);
    if (err != NULL) {
        return err;
    }
    int* jump_array = ALLOCATE(int, stmt->elif_branch_count + 1);
    // 先发出虚假偏移量的跳转指令
    int jump_if_false_offset =
        DaiCompiler_emit2(compiler, DaiOpJumpIfFalse, JUMP_PLACEHOLDER, stmt->start_line);
    err = DaiCompiler_compile(compiler, (DaiAstBase*)stmt->then_branch);
    if (err != NULL) {
        return err;
    }
    for (int i = 0; i < stmt->elif_branch_count; i++) {
        // 先发出虚假偏移量的跳转指令
        jump_array[i] = DaiCompiler_emit2(compiler, DaiOpJump, JUMP_PLACEHOLDER, stmt->start_line);
        DaiCompiler_patchJump(compiler, jump_if_false_offset);
        err = DaiCompiler_compile(compiler, (DaiAstBase*)stmt->elif_branches[i].condition);
        if (err != NULL) {
            return err;
        }
        // 先发出虚假偏移量的跳转指令
        jump_if_false_offset =
            DaiCompiler_emit2(compiler, DaiOpJumpIfFalse, JUMP_PLACEHOLDER, stmt->start_line);
        err = DaiCompiler_compile(compiler, (DaiAstBase*)stmt->elif_branches[i].then_branch);
        if (err != NULL) {
            return err;
        }
    }
    if (stmt->else_branch == NULL) {
        DaiCompiler_patchJump(compiler, jump_if_false_offset);
    } else {
        // 先发出虚假偏移量的跳转指令
        int jump_offset =
            DaiCompiler_emit2(compiler, DaiOpJump, JUMP_PLACEHOLDER, stmt->then_branch->start_line);
        DaiCompiler_patchJump(compiler, jump_if_false_offset);
        err = DaiCompiler_compile(compiler, (DaiAstBase*)stmt->else_branch);
        if (err != NULL) {
            return err;
        }
        DaiCompiler_patchJump(compiler, jump_offset);
    }
    for (int i = 0; i < stmt->elif_branch_count; i++) {
        DaiCompiler_patchJump(compiler, jump_array[i]);
    }
    FREE_ARRAY(int, jump_array, stmt->elif_branch_count + 1);
    IntArray_pop(&compiler->scope_stack);
    return NULL;
}
static DaiCompileError*
DaiCompiler_compileBlockStatement(DaiCompiler* compiler, DaiAstBase* node) {
    DaiSymbolTable* outer            = compiler->symbolTable;
    DaiSymbolTable* blockSymbolTable = DaiSymbolTable_NewEnclosed(outer);
    compiler->symbolTable            = blockSymbolTable;
    DaiAstBlockStatement* stmt       = (DaiAstBlockStatement*)node;
    for (int i = 0; i < stmt->length; i++) {
        DaiCompileError* err = DaiCompiler_compile(compiler, (DaiAstBase*)stmt->statements[i]);
        if (err != NULL) {
            DaiSymbolTable_free(blockSymbolTable);
            return err;
        }
    }
    compiler->is_function_block = false;
    DaiSymbolTable_free(blockSymbolTable);
    compiler->symbolTable = outer;
    return NULL;
}
static DaiCompileError*
DaiCompiler_compileAssignStatement(DaiCompiler* compiler, DaiAstBase* node) {
    DaiAstAssignStatement* stmt = (DaiAstAssignStatement*)node;
    if (stmt->operator!= NULL) {
        DaiCompileError* err = DaiCompiler_compile(compiler, (DaiAstBase*)stmt->left);
        if (err != NULL) {
            return err;
        }
    }
    DaiCompileError* err = DaiCompiler_compile(compiler, (DaiAstBase*)stmt->value);
    if (err != NULL) {
        return err;
    }
    if (stmt->operator!= NULL) {
        switch (*stmt->operator) {
            case '+': DaiCompiler_emit(compiler, DaiOpAdd, stmt->start_line); break;
            case '-': DaiCompiler_emit(compiler, DaiOpSub, stmt->start_line); break;
            case '*': DaiCompiler_emit(compiler, DaiOpMul, stmt->start_line); break;
            case '/': DaiCompiler_emit(compiler, DaiOpDiv, stmt->start_line); break;
        }
    }
    switch (stmt->left->type) {
        case DaiAstType_Identifier: {
            DaiAstIdentifier* ident = (DaiAstIdentifier*)stmt->left;
            DaiSymbol symbol;
            bool found = DaiSymbolTable_resolve(compiler->symbolTable, ident->value, &symbol);
            if (!found || !symbol.defined) {
                return DaiCompileError_Newf(compiler->filename,
                                            ident->start_line,
                                            ident->start_column,
                                            "undefined variable: '%s'",
                                            ident->value);
            }
            if (symbol.is_const) {
                return DaiCompileError_Newf(compiler->filename,
                                            ident->start_line,
                                            ident->start_column,
                                            "cannot assign to const variable '%s'",
                                            ident->value);
            }
            switch (symbol.type) {
                case DaiSymbolType_global: {
                    DaiCompiler_emit2(compiler, DaiOpSetGlobal, symbol.index, stmt->start_line);
                    ADD_GLOBAL_NAME(compiler, symbol.name);

                    break;
                }
                case DaiSymbolType_local: {
                    DaiCompiler_emit1(compiler, DaiOpSetLocal, symbol.index, stmt->start_line);
                    ADD_LOCAL_NAME(compiler, symbol.name);
                    break;
                }

                default: {
                    unreachable();
                }
            }

            break;
        }
        case DaiAstType_DotExpression: {
            DaiAstDotExpression* expr = (DaiAstDotExpression*)stmt->left;
            DaiCompileError* err2     = DaiCompiler_compile(compiler, (DaiAstBase*)expr->left);
            if (err2 != NULL) {
                return err2;
            }
            DaiObjString* name =
                dai_copy_string_intern(compiler->vm, expr->name->value, strlen(expr->name->value));
            int index = DaiChunk_addConstant(compiler->chunk, OBJ_VAL(name));
            DaiCompiler_emit2(compiler, DaiOpSetProperty, index, stmt->start_line);
            break;
        }
        case DaiAstType_ClassExpression: {
            DaiAstClassExpression* expr = (DaiAstClassExpression*)stmt->left;
            if (compiler->type != FunctionType_method &&
                compiler->type != FunctionType_classMethod) {
                return DaiCompileError_Newf(compiler->filename,
                                            expr->start_line,
                                            expr->start_column,
                                            "cannot use 'class' outside of a method");
            }
            if (compiler->type == FunctionType_classMethod) {
                // 在类方法里面， class 类似于 self
                DaiObjString* name = dai_copy_string_intern(
                    compiler->vm, expr->name->value, strlen(expr->name->value));
                int index = DaiChunk_addConstant(compiler->chunk, OBJ_VAL(name));
                DaiCompiler_emit2(compiler, DaiOpSetSelfProperty, index, stmt->start_line);
            } else {
                // 在实例方法里面，我们要从 self 里面获取类对象
                DaiObjString* name_class =
                    dai_copy_string_intern(compiler->vm, "__class__", strlen("__class__"));
                int index = DaiChunk_addConstant(compiler->chunk, OBJ_VAL(name_class));
                DaiCompiler_emit2(compiler, DaiOpGetSelfProperty, index, expr->start_line);


                DaiObjString* name = dai_copy_string_intern(
                    compiler->vm, expr->name->value, strlen(expr->name->value));
                index = DaiChunk_addConstant(compiler->chunk, OBJ_VAL(name));
                DaiCompiler_emit2(compiler, DaiOpSetProperty, index, stmt->start_line);
            }
            break;
        }
        case DaiAstType_SelfExpression: {
            DaiAstSelfExpression* expr = (DaiAstSelfExpression*)stmt->left;
            if (compiler->type != FunctionType_method) {
                return DaiCompileError_Newf(compiler->filename,
                                            expr->start_line,
                                            expr->start_column,
                                            "cannot use 'self' outside of a method");
            }
            DaiObjString* name =
                dai_copy_string_intern(compiler->vm, expr->name->value, strlen(expr->name->value));
            int index = DaiChunk_addConstant(compiler->chunk, OBJ_VAL(name));
            DaiCompiler_emit2(compiler, DaiOpSetSelfProperty, index, stmt->start_line);
            break;
        }
        case DaiAstType_SubscriptExpression: {
            DaiAstSubscriptExpression* expr = (DaiAstSubscriptExpression*)stmt->left;
            err = DaiCompiler_compile(compiler, (DaiAstBase*)expr->left);
            if (err != NULL) {
                return err;
            }
            err = DaiCompiler_compile(compiler, (DaiAstBase*)expr->right);
            if (err != NULL) {
                return err;
            }
            DaiCompiler_emit(compiler, DaiOpSubscriptSet, stmt->start_line);
            break;
        }
        default: {
            dai_error("assign unsupported %s\n", DaiAstType_string(stmt->left->type));
            unreachable();
        }
    }
    return NULL;
}
static DaiCompileError*
DaiCompiler_compileFunctionStatement(DaiCompiler* compiler, DaiAstBase* node) {
    IntArray_push(&compiler->scope_stack, ScopeType_function);
    DaiAstFunctionStatement* stmt = (DaiAstFunctionStatement*)node;
    if (DaiSymbolTable_isDefined(compiler->symbolTable, stmt->name)) {
        return DaiCompileError_Newf(compiler->filename,
                                    stmt->start_line,
                                    stmt->start_column,
                                    "symbol '%s' already defined",
                                    stmt->name);
    }
    DaiCompileError* err = DaiCompiler_compileFunction(compiler, node);
    if (err != NULL) {
        return err;
    }
    DaiSymbol symbol = DaiSymbolTable_define(compiler->symbolTable, stmt->name, true);
    switch (symbol.type) {
        case DaiSymbolType_global: {
            DaiCompiler_emit2(compiler, DaiOpDefineGlobal, symbol.index, stmt->start_line);
            ADD_GLOBAL_NAME(compiler, symbol.name);
            break;
        }
        case DaiSymbolType_local: {
            if (symbol.index >= LOCAL_MAX) {
                return DaiCompileError_Newf(compiler->filename,
                                            stmt->start_line,
                                            stmt->start_column,
                                            "error2 too many local symbols");
            }
            compiler->max_local_count = MAX(compiler->max_local_count, symbol.index + 1);
            DaiCompiler_emit1(compiler, DaiOpSetLocal, symbol.index, stmt->start_line);
            ADD_LOCAL_NAME(compiler, symbol.name);
            break;
        }
        default: {
            unreachable();
        }
    }
    IntArray_pop(&compiler->scope_stack);
    return NULL;
}
static DaiCompileError*
DaiCompiler_compileClassStatement(DaiCompiler* compiler, DaiAstBase* node) {
    IntArray_push(&compiler->scope_stack, ScopeType_class);
    DaiAstClassStatement* stmt = (DaiAstClassStatement*)node;
    if (DaiSymbolTable_isDefined(compiler->symbolTable, stmt->name)) {
        return DaiCompileError_Newf(compiler->filename,
                                    stmt->start_line,
                                    stmt->start_column,
                                    "symbol '%s' already defined",
                                    stmt->name);
    }
    DaiObjString* name = dai_copy_string_intern(compiler->vm, stmt->name, strlen(stmt->name));
    int index          = DaiCompiler_addConstant(compiler, OBJ_VAL(name));
    DaiCompiler_emit2(compiler, DaiOpClass, index, stmt->start_line);
    if (stmt->parent != NULL) {
        DaiCompileError* err = DaiCompiler_compile(compiler, (DaiAstBase*)stmt->parent);
        if (err != NULL) {
            return err;
        }
        DaiCompiler_emit(compiler, DaiOpInherit, stmt->start_line);
    }
    DaiSymbol symbol = DaiSymbolTable_define(compiler->symbolTable, stmt->name, true);
    switch (symbol.type) {
        case DaiSymbolType_global: {
            DaiCompiler_emit2(compiler, DaiOpDefineGlobal, symbol.index, stmt->start_line);
            ADD_GLOBAL_NAME(compiler, symbol.name);
            break;
        }
        case DaiSymbolType_local: {
            if (symbol.index >= LOCAL_MAX) {
                return DaiCompileError_Newf(compiler->filename,
                                            stmt->start_line,
                                            stmt->start_column,
                                            "error3 too many local symbols");
            }
            compiler->max_local_count = MAX(compiler->max_local_count, symbol.index + 1);
            DaiCompiler_emit1(compiler, DaiOpSetLocal, symbol.index, stmt->start_line);
            ADD_LOCAL_NAME(compiler, symbol.name);
            break;
        }
        default: {
            unreachable();
        }
    }
    // 加载类对象到栈顶
    DaiCompileError* err = DaiCompiler_loadSymbol(compiler, &symbol, stmt->start_line);
    if (err != NULL) {
        return err;
    }
    err = DaiCompiler_compile(compiler, (DaiAstBase*)stmt->body);
    if (err != NULL) {
        return err;
    }
    DaiTable_reset(&compiler->propertys);   // 重置属性表
    // 弹出栈顶的类对象
    DaiCompiler_emit(compiler, DaiOpPop, stmt->start_line);
    IntArray_pop(&compiler->scope_stack);
    return NULL;
}
static DaiCompileError*
DaiCompiler_compileInsVarStatement(DaiCompiler* compiler, DaiAstBase* node) {
    DaiAstInsVarStatement* stmt = (DaiAstInsVarStatement*)node;
    if (stmt->value != NULL) {
        DaiCompileError* err = DaiCompiler_compile(compiler, (DaiAstBase*)stmt->value);
        if (err != NULL) {
            return err;
        }
    } else {
        DaiCompiler_emit(compiler, DaiOpUndefined, stmt->start_line);
    }
    DaiObjString* name =
        dai_copy_string_intern(compiler->vm, stmt->name->value, strlen(stmt->name->value));
    if (DaiTable_has(&compiler->propertys, name)) {
        return DaiCompileError_Newf(compiler->filename,
                                    stmt->name->start_line,
                                    stmt->name->start_column,
                                    "property '%s' already defined",
                                    stmt->name->value);
    }
    DaiTable_set(&compiler->propertys, name, INTEGER_VAL(0));
    int index = DaiChunk_addConstant(compiler->chunk, OBJ_VAL(name));
    DaiCompiler_emit3(compiler, DaiOpDefineField, index, stmt->is_con, stmt->name->start_line);
    return NULL;
}
static DaiCompileError*
DaiCompiler_compileMethodStatement(DaiCompiler* compiler, DaiAstBase* node) {
    IntArray_push(&compiler->scope_stack, ScopeType_method);
    DaiAstMethodStatement* stmt = (DaiAstMethodStatement*)node;
    DaiObjString* name = dai_copy_string_intern(compiler->vm, stmt->name, strlen(stmt->name));
    if (DaiTable_has(&compiler->propertys, name)) {
        return DaiCompileError_Newf(compiler->filename,
                                    stmt->start_line,
                                    stmt->start_column,
                                    "property '%s' already defined",
                                    stmt->name);
    }
    DaiCompileError* err = DaiCompiler_compileFunction(compiler, node);
    if (err != NULL) {
        return err;
    }
    DaiTable_set(&compiler->propertys, name, INTEGER_VAL(0));
    int index = DaiChunk_addConstant(compiler->chunk, OBJ_VAL(name));
    DaiCompiler_emit2(compiler, DaiOpDefineMethod, index, stmt->start_line);
    IntArray_pop(&compiler->scope_stack);
    return NULL;
}
static DaiCompileError*
DaiCompiler_compileClassVarStatement(DaiCompiler* compiler, DaiAstBase* node) {
    DaiAstClassVarStatement* stmt = (DaiAstClassVarStatement*)node;
    DaiCompileError* err          = DaiCompiler_compile(compiler, (DaiAstBase*)stmt->value);
    if (err != NULL) {
        return err;
    }
    DaiObjString* name =
        dai_copy_string_intern(compiler->vm, stmt->name->value, strlen(stmt->name->value));
    if (DaiTable_has(&compiler->propertys, name)) {
        return DaiCompileError_Newf(compiler->filename,
                                    stmt->start_line,
                                    stmt->start_column,
                                    "property '%s' already defined",
                                    stmt->name->value);
    }
    DaiTable_set(&compiler->propertys, name, INTEGER_VAL(0));
    int index = DaiChunk_addConstant(compiler->chunk, OBJ_VAL(name));
    DaiCompiler_emit3(compiler, DaiOpDefineClassField, index, stmt->is_con, stmt->start_line);
    return NULL;
}
static DaiCompileError*
DaiCompiler_compileClassMethodStatement(DaiCompiler* compiler, DaiAstBase* node) {
    IntArray_push(&compiler->scope_stack, ScopeType_method);
    DaiAstClassMethodStatement* stmt = (DaiAstClassMethodStatement*)node;
    DaiCompileError* err             = DaiCompiler_compileFunction(compiler, node);
    if (err != NULL) {
        return err;
    }
    DaiObjString* name = dai_copy_string_intern(compiler->vm, stmt->name, strlen(stmt->name));
    int index          = DaiChunk_addConstant(compiler->chunk, OBJ_VAL(name));
    DaiCompiler_emit2(compiler, DaiOpDefineClassMethod, index, stmt->start_line);
    IntArray_pop(&compiler->scope_stack);
    return NULL;
}
static DaiCompileError*
DaiCompiler_compileWhileStatement(DaiCompiler* compiler, DaiAstBase* node) {
    IntArray_push(&compiler->scope_stack, ScopeType_while);
    DaiCompiler_enterLoop(compiler);
    int while_start            = compiler->chunk->count;
    DaiAstWhileStatement* stmt = (DaiAstWhileStatement*)node;
    DaiCompileError* err       = DaiCompiler_compile(compiler, (DaiAstBase*)stmt->condition);
    if (err != NULL) {
        return err;
    }
    // 先发出虚假偏移量的跳转指令
    int jump_if_false_offset =
        DaiCompiler_emit2(compiler, DaiOpJumpIfFalse, JUMP_PLACEHOLDER, stmt->start_line);
    err = DaiCompiler_compile(compiler, (DaiAstBase*)stmt->body);
    if (err != NULL) {
        return err;
    }
    {
        int jump_back =
            DaiCompiler_emit2(compiler, DaiOpJumpBack, JUMP_PLACEHOLDER, stmt->start_line);
        DaiCompiler_patchJumpBack(compiler, jump_back, while_start);
        DaiCompiler_patchJump(compiler, jump_if_false_offset);
    }
    DaiCompiler_leaveLoop(compiler, while_start);
    IntArray_pop(&compiler->scope_stack);
    return NULL;
}
static DaiCompileError*
DaiCompiler_compileContinueStatement(DaiCompiler* compiler, DaiAstBase* node) {
    DaiAstContinueStatement* stmt = (DaiAstContinueStatement*)node;
    if (!IntArray_contains(&compiler->scope_stack, ScopeType_while) &&
        !IntArray_contains(&compiler->scope_stack, ScopeType_for)) {
        return DaiCompileError_Newf(compiler->filename,
                                    stmt->start_line,
                                    stmt->start_column,
                                    "cannot use 'continue' outside of a loop");
    }
    int jump = DaiCompiler_emit2(compiler, DaiOpJumpBack, JUMP_PLACEHOLDER, stmt->start_line);
    IntArray_push(&compiler->continue_array, jump);
    return NULL;
}
static DaiCompileError*
DaiCompiler_compileBreakStatement(DaiCompiler* compiler, DaiAstBase* node) {
    DaiAstBreakStatement* stmt = (DaiAstBreakStatement*)node;
    if (!IntArray_contains(&compiler->scope_stack, ScopeType_while) &&
        !IntArray_contains(&compiler->scope_stack, ScopeType_for)) {
        return DaiCompileError_Newf(compiler->filename,
                                    stmt->start_line,
                                    stmt->start_column,
                                    "cannot use 'break' outside of a loop");
    }
    int jump = DaiCompiler_emit2(compiler, DaiOpJump, JUMP_PLACEHOLDER, stmt->start_line);
    IntArray_push(&compiler->break_array, jump);
    return NULL;
}
static DaiCompileError*
DaiCompiler_compileForInStatement(DaiCompiler* compiler, DaiAstBase* node) {
    IntArray_push(&compiler->scope_stack, ScopeType_for);
    DaiCompiler_enterLoop(compiler);
    DaiAstForInStatement* stmt = (DaiAstForInStatement*)node;
    DaiCompileError* err       = DaiCompiler_compile(compiler, (DaiAstBase*)stmt->expression);
    if (err != NULL) {
        return err;
    }
    // 定义迭代器和迭代循环变量
    DaiSymbolTable* outer            = compiler->symbolTable;
    DaiSymbolTable* blockSymbolTable = DaiSymbolTable_NewEnclosed(outer);
    compiler->symbolTable            = blockSymbolTable;
    DaiSymbol iterator_symbol =
        DaiSymbolTable_define(blockSymbolTable, "//iterator", true);   // 一个特殊的变量名表示迭代器
    if (stmt->i == NULL) {
        DaiSymbolTable_define(blockSymbolTable, "//i", true);   // 占个位置保持一致
    } else {
        DaiSymbolTable_define(blockSymbolTable, stmt->i->value, false);
    }
    DaiSymbolTable_define(blockSymbolTable, stmt->e->value, false);
    compiler->max_local_count = MAX(compiler->max_local_count, iterator_symbol.index + 3);
    // 编译 for-in 循环
    DaiCompiler_emit1(compiler, DaiOpIterInit, iterator_symbol.index, stmt->start_line);
    int for_start = compiler->chunk->count;
    int jump_if_end_offset =
        DaiCompiler_emitIterNext(compiler, iterator_symbol.index, stmt->start_line);
    err = DaiCompiler_compile(compiler, (DaiAstBase*)stmt->body);
    if (err != NULL) {
        return err;
    }
    {
        // 跳转到循环开始
        int jump_back =
            DaiCompiler_emit2(compiler, DaiOpJumpBack, JUMP_PLACEHOLDER, stmt->start_line);
        DaiCompiler_patchJumpBack(compiler, jump_back, for_start);
        // 更新 jump 到循环末尾的偏移量
        // DaiOpIterNext 比 jump 指令长度多 1 ，所以这里加 1 适配 jump 格式
        DaiCompiler_patchJump(compiler, jump_if_end_offset + 1);
    }
    DaiCompiler_leaveLoop(compiler, for_start);
    IntArray_pop(&compiler->scope_stack);
    DaiSymbolTable_free(blockSymbolTable);
    compiler->symbolTable = outer;
    return NULL;
}
static DaiCompileError*
DaiCompiler_compileIntegerLiteral(DaiCompiler* compiler, DaiAstBase* node) {
    DaiAstIntegerLiteral* lit = (DaiAstIntegerLiteral*)node;
    DaiValue integer          = INTEGER_VAL(lit->value);
    DaiCompiler_emit2(
        compiler, DaiOpConstant, DaiCompiler_addConstant(compiler, integer), lit->start_line);
    return NULL;
}
static DaiCompileError*
DaiCompiler_compileFloatLiteral(DaiCompiler* compiler, DaiAstBase* node) {
    DaiAstFloatLiteral* lit = (DaiAstFloatLiteral*)node;
    DaiValue num            = FLOAT_VAL(lit->value);
    DaiCompiler_emit2(
        compiler, DaiOpConstant, DaiCompiler_addConstant(compiler, num), lit->start_line);

    return NULL;
}
static DaiCompileError*
DaiCompiler_compileBoolean(DaiCompiler* compiler, DaiAstBase* node) {
    DaiAstBoolean* lit = (DaiAstBoolean*)node;
    if (lit->value) {
        DaiCompiler_emit(compiler, DaiOpTrue, lit->start_line);
    } else {
        DaiCompiler_emit(compiler, DaiOpFalse, lit->start_line);
    }
    return NULL;
}
static DaiCompileError*
DaiCompiler_compileNil(DaiCompiler* compiler, DaiAstBase* node) {
    DaiAstNil* lit = (DaiAstNil*)node;
    DaiCompiler_emit(compiler, DaiOpNil, lit->start_line);
    return NULL;
}
static DaiCompileError*
DaiCompiler_compileIdentifier(DaiCompiler* compiler, DaiAstBase* node) {
    DaiAstIdentifier* id = (DaiAstIdentifier*)node;
    DaiSymbol symbol;
    bool found = DaiSymbolTable_resolve(compiler->symbolTable, id->value, &symbol);
    // 全局变量可能是预定义状态
    if (!found || (compiler->type == FunctionType_script && !symbol.defined)) {
        return DaiCompileError_Newf(compiler->filename,
                                    id->start_line,
                                    id->start_column,
                                    "undefined variable: '%s'",
                                    id->value);
    }
    DaiCompileError* err = DaiCompiler_loadSymbol(compiler, &symbol, id->start_line);
    if (err != NULL) {
        return err;
    }
    return NULL;
}
static DaiCompileError*
DaiCompiler_compilePrefixExpression(DaiCompiler* compiler, DaiAstBase* node) {
    DaiAstPrefixExpression* expr = (DaiAstPrefixExpression*)node;
    DaiCompileError* err         = DaiCompiler_compile(compiler, (DaiAstBase*)expr->right);
    if (err != NULL) {
        return err;
    }
    if (strcmp(expr->operator, "-") == 0) {
        DaiCompiler_emit(compiler, DaiOpMinus, expr->start_line);
    } else if (strcmp(expr->operator, "!") == 0) {
        DaiCompiler_emit(compiler, DaiOpBang, expr->start_line);
    } else if (strcmp(expr->operator, "not") == 0) {
        DaiCompiler_emit(compiler, DaiOpNot, expr->start_line);
    } else if (strcmp(expr->operator, "~") == 0) {
        DaiCompiler_emit(compiler, DaiOpBitwiseNot, expr->start_line);
    } else {
        return DaiCompileError_Newf(compiler->filename,
                                    expr->start_line,
                                    expr->start_column,
                                    "unknown prefix operator: '%s'",
                                    expr->operator);
    }
    return NULL;
}
static DaiCompileError*
DaiCompiler_compileInfixExpression(DaiCompiler* compiler, DaiAstBase* node) {
    DaiAstInfixExpression* expr = (DaiAstInfixExpression*)node;
    if (strcmp(expr->operator, "and") == 0 || strcmp(expr->operator, "or") == 0) {
        return DaiCompiler_compileAndOrExpr(compiler, expr);
    }

    if (strcmp(expr->operator, "<") == 0) {
        // 交换左右两边
        DaiCompileError* err = DaiCompiler_compile(compiler, (DaiAstBase*)expr->right);
        if (err != NULL) {
            return err;
        }
        err = DaiCompiler_compile(compiler, (DaiAstBase*)expr->left);
        if (err != NULL) {
            return err;
        }
        DaiCompiler_emit(compiler, DaiOpGreaterThan, expr->start_line);
        return NULL;
    }
    if (strcmp(expr->operator, "<=") == 0) {
        // 交换左右两边
        DaiCompileError* err = DaiCompiler_compile(compiler, (DaiAstBase*)expr->right);
        if (err != NULL) {
            return err;
        }
        err = DaiCompiler_compile(compiler, (DaiAstBase*)expr->left);
        if (err != NULL) {
            return err;
        }
        DaiCompiler_emit(compiler, DaiOpGreaterEqualThan, expr->start_line);
        return NULL;
    }

    DaiCompileError* err = DaiCompiler_compile(compiler, (DaiAstBase*)expr->left);
    if (err != NULL) {
        return err;
    }
    err = DaiCompiler_compile(compiler, (DaiAstBase*)expr->right);
    if (err != NULL) {
        return err;
    }
    if (strcmp(expr->operator, "+") == 0) {
        DaiCompiler_emit(compiler, DaiOpAdd, expr->start_line);
    } else if (strcmp(expr->operator, "-") == 0) {
        DaiCompiler_emit(compiler, DaiOpSub, expr->start_line);
    } else if (strcmp(expr->operator, "*") == 0) {
        DaiCompiler_emit(compiler, DaiOpMul, expr->start_line);
    } else if (strcmp(expr->operator, "/") == 0) {
        DaiCompiler_emit(compiler, DaiOpDiv, expr->start_line);
    } else if (strcmp(expr->operator, ">") == 0) {
        DaiCompiler_emit(compiler, DaiOpGreaterThan, expr->start_line);
    } else if (strcmp(expr->operator, ">=") == 0) {
        DaiCompiler_emit(compiler, DaiOpGreaterEqualThan, expr->start_line);
    } else if (strcmp(expr->operator, "==") == 0) {
        DaiCompiler_emit(compiler, DaiOpEqual, expr->start_line);
    } else if (strcmp(expr->operator, "!=") == 0) {
        DaiCompiler_emit(compiler, DaiOpNotEqual, expr->start_line);
    } else if (strcmp(expr->operator, "%") == 0) {
        DaiCompiler_emit(compiler, DaiOpMod, expr->start_line);
    } else if (strcmp(expr->operator, "<<") == 0) {
        DaiCompiler_emit1(compiler, DaiOpBinary, BinaryOpLeftShift, expr->start_line);
    } else if (strcmp(expr->operator, ">>") == 0) {
        DaiCompiler_emit1(compiler, DaiOpBinary, BinaryOpRightShift, expr->start_line);
    } else if (strcmp(expr->operator, "&") == 0) {
        DaiCompiler_emit1(compiler, DaiOpBinary, BinaryOpBitwiseAnd, expr->start_line);
    } else if (strcmp(expr->operator, "^") == 0) {
        DaiCompiler_emit1(compiler, DaiOpBinary, BinaryOpBitwiseXor, expr->start_line);
    } else if (strcmp(expr->operator, "|") == 0) {
        DaiCompiler_emit1(compiler, DaiOpBinary, BinaryOpBitwiseOr, expr->start_line);
    } else {
        return DaiCompileError_Newf(compiler->filename,
                                    expr->left->end_line,
                                    expr->left->end_column + 1,
                                    "unknown infix operator: '%s'",
                                    expr->operator);
    }
    return NULL;
}
static DaiCompileError*
DaiCompiler_compileFunctionLiteral(DaiCompiler* compiler, DaiAstBase* node) {
    return DaiCompiler_compileFunction(compiler, node);
}
static DaiCompileError*
DaiCompiler_compileStringLiteral(DaiCompiler* compiler, DaiAstBase* node) {
    DaiAstStringLiteral* lit = (DaiAstStringLiteral*)node;
    // +1 -2 是为了去掉字符串前后的引号
    DaiObjString* string =
        dai_copy_string_intern(compiler->vm, lit->value + 1, strlen(lit->value) - 2);
    DaiCompiler_emit2(compiler,
                      DaiOpConstant,
                      DaiCompiler_addConstant(compiler, OBJ_VAL(string)),
                      lit->start_line);
    return NULL;
}
static DaiCompileError*
DaiCompiler_compileArrayLiteral(DaiCompiler* compiler, DaiAstBase* node) {
    DaiAstArrayLiteral* lit = (DaiAstArrayLiteral*)node;
    for (int i = 0; i < lit->length; i++) {
        DaiCompileError* err = DaiCompiler_compile(compiler, (DaiAstBase*)lit->elements[i]);
        if (err != NULL) {
            return err;
        }
    }
    DaiCompiler_emit2(compiler, DaiOpArray, lit->length, lit->start_line);
    return NULL;
}
static DaiCompileError*
DaiCompiler_compileCallExpression(DaiCompiler* compiler, DaiAstBase* node) {
    DaiAstCallExpression* expr = (DaiAstCallExpression*)node;
    if (expr->arguments_count >= LOCAL_MAX) {
        return DaiCompileError_Newf(compiler->filename,
                                    expr->start_line,
                                    expr->start_column,
                                    "too many arguments to call, got %d",
                                    (int)expr->arguments_count);
    }
    switch (expr->function->type) {
        case DaiAstType_DotExpression: {
            DaiAstDotExpression* dot = (DaiAstDotExpression*)expr->function;
            DaiCompileError* err     = DaiCompiler_compile(compiler, (DaiAstBase*)dot->left);
            if (err != NULL) {
                return err;
            }
            for (int i = 0; i < expr->arguments_count; i++) {
                err = DaiCompiler_compile(compiler, (DaiAstBase*)expr->arguments[i]);
                if (err != NULL) {
                    return err;
                }
            }
            DaiObjString* name =
                dai_copy_string_intern(compiler->vm, dot->name->value, strlen(dot->name->value));
            int index = DaiChunk_addConstant(compiler->chunk, OBJ_VAL(name));
            DaiCompiler_emit3(
                compiler, DaiOpCallMethod, index, expr->arguments_count, expr->start_line);
            return NULL;
        }
        case DaiAstType_ClassExpression: {
            if (compiler->type != FunctionType_method &&
                compiler->type != FunctionType_classMethod) {
                return DaiCompileError_Newf(compiler->filename,
                                            expr->start_line,
                                            expr->start_column,
                                            "cannot use 'class' outside of a method");
            }
            DaiCompiler_emit(compiler, DaiOpNil, expr->start_line);
            for (int i = 0; i < expr->arguments_count; i++) {
                DaiCompileError* err =
                    DaiCompiler_compile(compiler, (DaiAstBase*)expr->arguments[i]);
                if (err != NULL) {
                    return err;
                }
            }
            DaiAstClassExpression* classexpr = (DaiAstClassExpression*)expr->function;
            if (compiler->type == FunctionType_classMethod) {
                // 在类方法里面， class 类似于 self
                DaiObjString* name = dai_copy_string_intern(
                    compiler->vm, classexpr->name->value, strlen(classexpr->name->value));
                int index = DaiChunk_addConstant(compiler->chunk, OBJ_VAL(name));
                DaiCompiler_emit3(
                    compiler, DaiOpCallSelfMethod, index, expr->arguments_count, expr->start_line);
            } else {
                // 在实例方法里面，我们要从 self 里面获取类对象
                DaiObjString* name_class =
                    dai_copy_string_intern(compiler->vm, "__class__", strlen("__class__"));
                int index = DaiChunk_addConstant(compiler->chunk, OBJ_VAL(name_class));
                DaiCompiler_emit2(compiler, DaiOpGetSelfProperty, index, expr->start_line);
                // 调用对象方法
                DaiObjString* name = dai_copy_string_intern(
                    compiler->vm, classexpr->name->value, strlen(classexpr->name->value));
                index = DaiChunk_addConstant(compiler->chunk, OBJ_VAL(name));
                DaiCompiler_emit3(
                    compiler, DaiOpCallMethod, index, expr->arguments_count, expr->start_line);
                return NULL;
            }
            return NULL;
        }
        case DaiAstType_SelfExpression: {
            if (compiler->type != FunctionType_method) {
                return DaiCompileError_Newf(compiler->filename,
                                            expr->start_line,
                                            expr->start_column,
                                            "cannot use 'self' outside of a method");
            }
            // 塞一个 nil 到栈顶，代替原本的函数对象入栈操作
            DaiCompiler_emit(compiler, DaiOpNil, expr->start_line);
            for (int i = 0; i < expr->arguments_count; i++) {
                DaiCompileError* err =
                    DaiCompiler_compile(compiler, (DaiAstBase*)expr->arguments[i]);
                if (err != NULL) {
                    return err;
                }
            }
            DaiAstSelfExpression* selfexpr = (DaiAstSelfExpression*)expr->function;
            DaiObjString* name             = dai_copy_string_intern(
                compiler->vm, selfexpr->name->value, strlen(selfexpr->name->value));
            int index = DaiChunk_addConstant(compiler->chunk, OBJ_VAL(name));
            DaiCompiler_emit3(
                compiler, DaiOpCallSelfMethod, index, expr->arguments_count, expr->start_line);
            return NULL;
        }
        case DaiAstType_SuperExpression: {
            DaiCompiler_emit(compiler, DaiOpNil, expr->start_line);
            for (int i = 0; i < expr->arguments_count; i++) {
                DaiCompileError* err =
                    DaiCompiler_compile(compiler, (DaiAstBase*)expr->arguments[i]);
                if (err != NULL) {
                    return err;
                }
            }
            DaiAstSelfExpression* superexpr = (DaiAstSelfExpression*)expr->function;
            DaiObjString* name              = dai_copy_string_intern(
                compiler->vm, superexpr->name->value, strlen(superexpr->name->value));
            int index = DaiChunk_addConstant(compiler->chunk, OBJ_VAL(name));
            DaiCompiler_emit3(
                compiler, DaiOpCallSuperMethod, index, expr->arguments_count, expr->start_line);
            return NULL;
        }
        default: {
            break;
        }
    }

    DaiCompileError* err = DaiCompiler_compile(compiler, (DaiAstBase*)expr->function);
    if (err != NULL) {
        return err;
    }
    for (int i = 0; i < expr->arguments_count; i++) {
        err = DaiCompiler_compile(compiler, (DaiAstBase*)expr->arguments[i]);
        if (err != NULL) {
            return err;
        }
    }
    DaiCompiler_emit1(compiler, DaiOpCall, expr->arguments_count, expr->start_line);
    return NULL;
}
static DaiCompileError*
DaiCompiler_compileDotExpression(DaiCompiler* compiler, DaiAstBase* node) {
    DaiAstDotExpression* expr = (DaiAstDotExpression*)node;
    DaiCompileError* err      = DaiCompiler_compile(compiler, (DaiAstBase*)expr->left);
    if (err != NULL) {
        return err;
    }
    DaiObjString* name =
        dai_copy_string_intern(compiler->vm, expr->name->value, strlen(expr->name->value));
    int index = DaiChunk_addConstant(compiler->chunk, OBJ_VAL(name));
    DaiCompiler_emit2(compiler, DaiOpGetProperty, index, expr->start_line);
    return NULL;
}
static DaiCompileError*
DaiCompiler_compileSelfExpression(DaiCompiler* compiler, DaiAstBase* node) {
    DaiAstSelfExpression* expr = (DaiAstSelfExpression*)node;
    if (compiler->type != FunctionType_method) {
        return DaiCompileError_Newf(compiler->filename,
                                    expr->start_line,
                                    expr->start_column,
                                    "cannot use 'self' outside of a method");
    }
    if (expr->name) {
        DaiObjString* name =
            dai_copy_string_intern(compiler->vm, expr->name->value, strlen(expr->name->value));
        int index = DaiChunk_addConstant(compiler->chunk, OBJ_VAL(name));
        DaiCompiler_emit2(compiler, DaiOpGetSelfProperty, index, expr->start_line);
    } else {
        DaiCompiler_emit1(compiler, DaiOpGetLocal, 0, expr->start_line);
    }
    return NULL;
}
static DaiCompileError*
DaiCompiler_compileClassExpression(DaiCompiler* compiler, DaiAstBase* node) {
    DaiAstClassExpression* expr = (DaiAstClassExpression*)node;
    if (compiler->type != FunctionType_method && compiler->type != FunctionType_classMethod) {
        return DaiCompileError_Newf(compiler->filename,
                                    expr->start_line,
                                    expr->start_column,
                                    "cannot use 'class' outside of a method");
    }
    if (compiler->type == FunctionType_classMethod) {
        // 在类方法里面， class 类似于 self
        if (expr->name) {
            DaiObjString* name =
                dai_copy_string_intern(compiler->vm, expr->name->value, strlen(expr->name->value));
            int index = DaiChunk_addConstant(compiler->chunk, OBJ_VAL(name));
            DaiCompiler_emit2(compiler, DaiOpGetSelfProperty, index, expr->start_line);
        } else {
            DaiCompiler_emit1(compiler, DaiOpGetLocal, 0, expr->start_line);
        }
    } else {
        // 在实例方法里面，我们要从 self 里面获取类对象
        DaiObjString* name_class =
            dai_copy_string_intern(compiler->vm, "__class__", strlen("__class__"));
        int index = DaiChunk_addConstant(compiler->chunk, OBJ_VAL(name_class));
        DaiCompiler_emit2(compiler, DaiOpGetSelfProperty, index, expr->start_line);
        if (expr->name) {
            DaiObjString* name =
                dai_copy_string_intern(compiler->vm, expr->name->value, strlen(expr->name->value));
            index = DaiChunk_addConstant(compiler->chunk, OBJ_VAL(name));
            DaiCompiler_emit2(compiler, DaiOpGetProperty, index, expr->start_line);
        }
    }
    return NULL;
}
static DaiCompileError*
DaiCompiler_compileSuperExpression(DaiCompiler* compiler, DaiAstBase* node) {
    DaiAstSuperExpression* expr = (DaiAstSuperExpression*)node;
    if (compiler->type != FunctionType_method && compiler->type != FunctionType_classMethod) {
        return DaiCompileError_Newf(compiler->filename,
                                    expr->start_line,
                                    expr->start_column,
                                    "cannot use 'super' outside of a method");
    }
    DaiObjString* name =
        dai_copy_string_intern(compiler->vm, expr->name->value, strlen(expr->name->value));
    int index = DaiChunk_addConstant(compiler->chunk, OBJ_VAL(name));
    DaiCompiler_emit2(compiler, DaiOpGetSuperProperty, index, expr->start_line);
    return NULL;
}
static DaiCompileError*
DaiCompiler_compileSubscriptExpression(DaiCompiler* compiler, DaiAstBase* node) {
    DaiAstSubscriptExpression* sub = (DaiAstSubscriptExpression*)node;
    DaiCompileError* err           = NULL;
    err                            = DaiCompiler_compile(compiler, (DaiAstBase*)sub->left);
    if (err != NULL) {
        return err;
    }
    err = DaiCompiler_compile(compiler, (DaiAstBase*)sub->right);
    if (err != NULL) {
        return err;
    }
    DaiCompiler_emit(compiler, DaiOpSubscript, sub->start_line);
    return NULL;
}
static DaiCompileError*
DaiCompiler_compileMapLiteral(DaiCompiler* compiler, DaiAstBase* node) {
    DaiAstMapLiteral* lit = (DaiAstMapLiteral*)node;
    for (int i = 0; i < lit->length; i++) {
        DaiCompileError* err = DaiCompiler_compile(compiler, (DaiAstBase*)lit->pairs[i].key);
        if (err != NULL) {
            return err;
        }
        err = DaiCompiler_compile(compiler, (DaiAstBase*)lit->pairs[i].value);
        if (err != NULL) {
            return err;
        }
    }
    DaiCompiler_emit2(compiler, DaiOpMap, lit->length, lit->start_line);
    return NULL;
}

typedef DaiCompileError* (*compile_ast_func)(DaiCompiler* compiler, DaiAstBase* node);
static compile_ast_func compile_ast_func_table[] = {
    [DaiAstType_program]              = DaiCompiler_compileProgram,
    [DaiAstType_VarStatement]         = DaiCompiler_compileVarStatement,
    [DaiAstType_ReturnStatement]      = DaiCompiler_compileReturnStatement,
    [DaiAstType_ExpressionStatement]  = DaiCompiler_compileExpressionStatement,
    [DaiAstType_IfStatement]          = DaiCompiler_compileIfStatement,
    [DaiAstType_BlockStatement]       = DaiCompiler_compileBlockStatement,
    [DaiAstType_AssignStatement]      = DaiCompiler_compileAssignStatement,
    [DaiAstType_FunctionStatement]    = DaiCompiler_compileFunctionStatement,
    [DaiAstType_ClassStatement]       = DaiCompiler_compileClassStatement,
    [DaiAstType_InsVarStatement]      = DaiCompiler_compileInsVarStatement,
    [DaiAstType_MethodStatement]      = DaiCompiler_compileMethodStatement,
    [DaiAstType_ClassVarStatement]    = DaiCompiler_compileClassVarStatement,
    [DaiAstType_ClassMethodStatement] = DaiCompiler_compileClassMethodStatement,
    [DaiAstType_WhileStatement]       = DaiCompiler_compileWhileStatement,
    [DaiAstType_ContinueStatement]    = DaiCompiler_compileContinueStatement,
    [DaiAstType_BreakStatement]       = DaiCompiler_compileBreakStatement,
    [DaiAstType_ForInStatement]       = DaiCompiler_compileForInStatement,
    [DaiAstType_IntegerLiteral]       = DaiCompiler_compileIntegerLiteral,
    [DaiAstType_FloatLiteral]         = DaiCompiler_compileFloatLiteral,
    [DaiAstType_Boolean]              = DaiCompiler_compileBoolean,
    [DaiAstType_Nil]                  = DaiCompiler_compileNil,
    [DaiAstType_Identifier]           = DaiCompiler_compileIdentifier,
    [DaiAstType_PrefixExpression]     = DaiCompiler_compilePrefixExpression,
    [DaiAstType_InfixExpression]      = DaiCompiler_compileInfixExpression,
    [DaiAstType_FunctionLiteral]      = DaiCompiler_compileFunctionLiteral,
    [DaiAstType_StringLiteral]        = DaiCompiler_compileStringLiteral,
    [DaiAstType_ArrayLiteral]         = DaiCompiler_compileArrayLiteral,
    [DaiAstType_CallExpression]       = DaiCompiler_compileCallExpression,
    [DaiAstType_DotExpression]        = DaiCompiler_compileDotExpression,
    [DaiAstType_SelfExpression]       = DaiCompiler_compileSelfExpression,
    [DaiAstType_ClassExpression]      = DaiCompiler_compileClassExpression,
    [DaiAstType_SuperExpression]      = DaiCompiler_compileSuperExpression,
    [DaiAstType_SubscriptExpression]  = DaiCompiler_compileSubscriptExpression,
    [DaiAstType_MapLiteral]           = DaiCompiler_compileMapLiteral,
};

static DaiCompileError*
DaiCompiler_compile(DaiCompiler* compiler, DaiAstBase* node) {
    return compile_ast_func_table[node->type](compiler, node);
}

// #endregion

static int
DaiCompiler_addConstant(const DaiCompiler* compiler, DaiValue value) {
    return DaiChunk_addConstant(compiler->chunk, value);
}

static int
DaiCompiler_emit(const DaiCompiler* compiler, DaiOpCode op, int line) {
    DaiChunk* chunk = compiler->chunk;
    DaiChunk_write(chunk, (uint8_t)op, line);
    return chunk->count - 1;
}

static int
DaiCompiler_emit1(const DaiCompiler* compiler, DaiOpCode op, uint8_t operand, int line) {
    DaiChunk* chunk = compiler->chunk;
    DaiChunk_write(chunk, (uint8_t)op, line);
    DaiChunk_write(chunk, operand, line);
    return chunk->count - 2;
}
static int
DaiCompiler_emit2(const DaiCompiler* compiler, DaiOpCode op, uint16_t operand, int line) {
    DaiChunk* chunk = compiler->chunk;
    DaiChunk_writeu16(chunk, op, operand, line);
    return chunk->count - 3;
}
static int
DaiCompiler_emit3(const DaiCompiler* compiler, DaiOpCode op, uint16_t operand1, uint8_t operand2,
                  int line) {
    DaiChunk* chunk = compiler->chunk;
    DaiChunk_writeu16(chunk, op, operand1, line);
    DaiChunk_write(chunk, operand2, line);
    return chunk->count - 4;
}

static int
DaiCompiler_emitIterNext(const DaiCompiler* compiler, uint8_t operand1, int line) {
    DaiChunk* chunk = compiler->chunk;
    DaiChunk_write(chunk, DaiOpIterNext, line);
    DaiChunk_write(chunk, operand1, line);
    DaiChunk_write2(chunk, 65535, line);
    return chunk->count - 4;
}

static void
DaiCompiler_patchJump(const DaiCompiler* compiler, int offset) {
    DaiChunk* chunk = compiler->chunk;
    // 减去3是因为 jump 指令占用3个字节
    int jump = chunk->count - offset - 3;

    if (jump > UINT16_MAX) {
        fprintf(stderr, "too much code to jump over\n");
        abort();
    }

    chunk->code[offset + 1] = (uint8_t)((jump >> 8) & 0xff);
    chunk->code[offset + 2] = (uint8_t)(jump & 0xff);
}

static void
DaiCompiler_patchJumpBack(const DaiCompiler* compiler, int offset, int dst) {
    DaiChunk* chunk = compiler->chunk;
    // 减去3是因为 jump 指令占用3个字节
    int jump = offset - dst + 3;

    if (jump > UINT16_MAX) {
        fprintf(stderr, "too much code to jump over\n");
        abort();
    }

    chunk->code[offset + 1] = (uint8_t)((jump >> 8) & 0xff);
    chunk->code[offset + 2] = (uint8_t)(jump & 0xff);
}

// #endregion

DaiCompileError*
dai_compile(DaiAstProgram* program, DaiObjModule* module, DaiVM* vm) {
    vm->state = VMState_compiling;
    DaiCompiler compiler;
    DaiSymbolTable* globalSymbolTable = DaiSymbolTable_New();

    DaiObjModule_beforeCompile(module, globalSymbolTable);
    DaiCompiler_init(&compiler, module, &module->chunk, globalSymbolTable, FunctionType_script, vm);
    IntArray_push(&compiler.scope_stack, ScopeType_script);
    DaiCompileError* err = NULL;
    err                  = DaiCompiler_extractSymbol(&compiler, (DaiAstBase*)program);
    if (err != NULL) {
        goto END;
    }
    err = DaiCompiler_compile(&compiler, (DaiAstBase*)program);
    // 局部变量是预先分配在栈上的，同样需要占用栈空间
    module->max_local_count = compiler.max_local_count;
    module->max_stack_size  = module->max_local_count + calculate_max_stack_size(&module->chunk);
    DaiObjModule_afterCompile(module, globalSymbolTable);
    DaiCompiler_emit(&compiler, DaiOpEnd, 0);

END:
    // free comiler
    DaiSymbolTable_free(globalSymbolTable);
    DaiCompiler_reset(&compiler);
    return err;
}
