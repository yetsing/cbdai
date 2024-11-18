#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "dai_assert.h"
#include "dai_compile.h"
#include "dai_memory.h"
#include "dai_object.h"
#include "dai_parse.h"

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
IntArray_contains(IntArray* array, int value) {
    for (int i = 0; i < array->count; i++) {
        if (array->values[i].value == value) {
            return true;
        }
    }
    return false;
}

void
IntArray_show(IntArray* array) {
    dai_log("IntArray level=%d, count=%d [\n", array->level, array->count);
    for (int i = 0; i < array->count; i++) {
        dai_log("    level=%d, value=%d\n", array->values[i].level, array->values[i].value);
    }
    dai_log("]\n");
}

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
    // function 不归 DaiCompiler 所有，仅作为编译器的一部分
    // 将整个脚本作为一个函数
    DaiObjFunction* function;

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

    IntArray scope_stack;   // 记录作用域
    IntArray break_array;   // 记录 break 跳转指令的位置，用于后续更新跳转偏移量
    IntArray continue_array;   // 记录 continue 跳转指令的位置，用于后续更新跳转偏移量

} DaiCompiler;

#ifdef DISASSEMBLE_VARIABLE_NAME
void
DaiCompiler_addName(DaiCompiler* compiler, const char* name, int back) {
    DaiChunk* chunk = &(compiler->function->chunk);
    DaiChunk_addName(chunk, name, back);
}
#    define ADD_GLOBAL_NAME(compiler, name) DaiCompiler_addName((compiler), (name), 3)
#    define ADD_LOCAL_NAME(compiler, name) DaiCompiler_addName((compiler), (name), 2)
#    define ADD_FREE_NAME(compiler, name) DaiCompiler_addName((compiler), (name), 2)

#else
#    define ADD_GLOBAL_NAME(compiler, name) \
        do {                                \
        } while (0)
#    define ADD_LOCAL_NAME(compiler, name) \
        do {                               \
        } while (0)
#    define ADD_FREE_NAME(compiler, name) \
        do {                              \
        } while (0)

#endif

static int
DaiCompiler_addConstant(DaiCompiler* compiler, DaiValue value);
static int
DaiCompiler_emit(DaiCompiler* compiler, DaiOpCode op, int line);
static int
DaiCompiler_emit1(DaiCompiler* compiler, DaiOpCode op, uint8_t operand, int line);
static int
DaiCompiler_emit2(DaiCompiler* compiler, DaiOpCode op, uint16_t operand, int line);
static int
DaiCompiler_emit3(DaiCompiler* compiler, DaiOpCode op, uint16_t operand1, uint8_t operand2,
                  int line);
static void
DaiCompiler_patchJump(DaiCompiler* compiler, int offset);
static void
DaiCompiler_patchJumpBack(DaiCompiler* compiler, int offset, int dst);
static void
DaiCompiler_reset(DaiCompiler* compiler);
static DaiCompileError*
DaiCompiler_compile(DaiCompiler* compiler, DaiAstBase* node);

static DaiCompileError*
DaiCompiler_loadSymbol(DaiCompiler* compiler, DaiSymbol* symbol, int start_line) {
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
            break;
        }
    }
    return NULL;
}

static void
DaiCompiler_init(DaiCompiler* compiler, DaiObjFunction* function, DaiSymbolTable* symbolTable,
                 FunctionType type, DaiVM* vm) {
    compiler->filename          = function->chunk.filename;
    compiler->function          = function;
    compiler->type              = type;
    compiler->vm                = vm;
    compiler->symbolTable       = symbolTable;
    compiler->is_function_block = false;
    DaiTable_init(&compiler->propertys);
    compiler->anonymous_count = 0;
    IntArray_init(&compiler->scope_stack);
    IntArray_init(&compiler->break_array);
    IntArray_init(&compiler->continue_array);
}

static void
DaiCompiler_reset(DaiCompiler* compiler) {
    DaiTable_reset(&compiler->propertys);
    IntArray_reset(&compiler->scope_stack);
    IntArray_reset(&compiler->break_array);
    IntArray_reset(&compiler->continue_array);
}

static DaiCompileError*
DaiCompiler_compileFunction(DaiCompiler* compiler, DaiAstBase* node) {
    // 函数名字
    const char* name;
    FunctionType function_type    = FunctionType_function;
    DaiAstBlockStatement* body    = NULL;
    int parameters_count          = 0;
    DaiAstIdentifier** parameters = NULL;
    int start_line                = 0;
    int end_line                  = 0;
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
            break;
        }
        default: {
            dai_error("Unknown function type: %d\n", node->type);
            unreachable();
            break;
        }
    }

    // 创建函数对象
    DaiObjFunction* function = DaiObjFunction_New(compiler->vm, name);
    // 创建函数符号表
    DaiSymbolTable* functable = DaiSymbolTable_NewFunction(compiler->symbolTable);
    // 创建子编译器
    DaiCompiler subcompiler;
    DaiCompiler_init(&subcompiler, function, functable, FunctionType_function, compiler->vm);
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
        DaiSymbolTable_define(functable, param->value);
    }
    // 编译函数体
    DaiCompileError* err = DaiCompiler_compile(&subcompiler, (DaiAstBase*)body);
    if (err != NULL) {
        return err;
    }
    // 给函数末尾统一加一个 return nil 指令，防止函数没有 return
    DaiCompiler_emit(&subcompiler, DaiOpReturn, end_line);
    int num_free = 0;
    {
        DaiSymbol* free_symbols = DaiSymbolTable_getFreeSymbols(functable, &num_free);
        for (int i = 0; i < num_free; i++) {
            DaiCompileError* err = DaiCompiler_loadSymbol(compiler, &free_symbols[i], start_line);
            if (err != NULL) {
                return err;
            }
        }
    }
    DaiSymbolTable_free(functable);
    DaiCompiler_reset(&subcompiler);

    DaiCompiler_emit3(compiler,
                      DaiOpClosure,
                      DaiCompiler_addConstant(compiler, OBJ_VAL(function)),
                      num_free,
                      start_line);
    compiler->anonymous_count = subcompiler.anonymous_count;
    return NULL;
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
    if (name != NULL) {
        DaiSymbol symbol;
        if (DaiSymbolTable_resolve(compiler->symbolTable, name, &symbol)) {
            return DaiCompileError_Newf(
                compiler->filename, line, column, "symbol '%s' already defined", name);
        }
        symbol = DaiSymbolTable_predefine(compiler->symbolTable, name);
        if (symbol.index >= UINT16_MAX) {
            return DaiCompileError_Newf(
                compiler->filename, line, column, "too many global symbols(MAX=%d)", UINT16_MAX);
        }
    }
    return NULL;
}

static DaiCompileError*
DaiCompiler_compile(DaiCompiler* compiler, DaiAstBase* node) {
    switch (node->type) {
        case DaiAstType_program: {
            DaiAstProgram* program = (DaiAstProgram*)node;
            for (int i = 0; i < program->length; i++) {
                DaiCompileError* err =
                    DaiCompiler_compile(compiler, (DaiAstBase*)program->statements[i]);
                if (err != NULL) {
                    return err;
                }
            }
            break;
        }
        case DaiAstType_BlockStatement: {
            DaiSymbolTable* outer            = compiler->symbolTable;
            DaiSymbolTable* blockSymbolTable = DaiSymbolTable_NewEnclosed(outer);
            compiler->symbolTable            = blockSymbolTable;
            DaiAstBlockStatement* stmt       = (DaiAstBlockStatement*)node;
            for (int i = 0; i < stmt->length; i++) {
                DaiCompileError* err =
                    DaiCompiler_compile(compiler, (DaiAstBase*)stmt->statements[i]);
                if (err != NULL) {
                    DaiSymbolTable_free(blockSymbolTable);
                    return err;
                }
            }
            // 函数体的 block 不进行 pop
            if (!compiler->is_function_block) {
                int count = DaiSymbolTable_count(blockSymbolTable);
                for (int i = 0; i < count; i++) {
                    DaiCompiler_emit(compiler, DaiOpPop, stmt->start_line);
                }
            }
            compiler->is_function_block = false;
            DaiSymbolTable_free(blockSymbolTable);
            compiler->symbolTable = outer;
            break;
        }
        case DaiAstType_IfStatement: {
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
                DaiCompiler_emit2(compiler, DaiOpJumpIfFalse, 9999, stmt->start_line);
            err = DaiCompiler_compile(compiler, (DaiAstBase*)stmt->then_branch);
            if (err != NULL) {
                return err;
            }
            for (int i = 0; i < stmt->elif_branch_count; i++) {
                // 先发出虚假偏移量的跳转指令
                jump_array[i] = DaiCompiler_emit2(compiler, DaiOpJump, 9999, stmt->start_line);
                DaiCompiler_patchJump(compiler, jump_if_false_offset);
                err = DaiCompiler_compile(compiler, (DaiAstBase*)stmt->elif_branches[i].condition);
                if (err != NULL) {
                    return err;
                }
                // 先发出虚假偏移量的跳转指令
                jump_if_false_offset =
                    DaiCompiler_emit2(compiler, DaiOpJumpIfFalse, 9999, stmt->start_line);
                err =
                    DaiCompiler_compile(compiler, (DaiAstBase*)stmt->elif_branches[i].then_branch);
                if (err != NULL) {
                    return err;
                }
            }
            if (stmt->else_branch == NULL) {
                DaiCompiler_patchJump(compiler, jump_if_false_offset);
            } else {
                // 先发出虚假偏移量的跳转指令
                int jump_offset =
                    DaiCompiler_emit2(compiler, DaiOpJump, 9999, stmt->then_branch->start_line);
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
            break;
        }
        case DaiAstType_VarStatement: {
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
            DaiSymbol symbol = DaiSymbolTable_define(compiler->symbolTable, stmt->name->value);
            switch (symbol.type) {
                case DaiSymbolType_global: {
                    DaiCompiler_emit2(compiler, DaiOpDefineGlobal, symbol.index, stmt->start_line);
                    ADD_GLOBAL_NAME(compiler, symbol.name);
                    break;
                }
                case DaiSymbolType_local: {
                    if (symbol.index >= UINT8_MAX) {
                        return DaiCompileError_Newf(compiler->filename,
                                                    stmt->name->start_line,
                                                    stmt->name->start_column,
                                                    "too many local symbols(MAX=%d)",
                                                    UINT8_MAX);
                    }
                    DaiCompiler_emit1(compiler, DaiOpSetLocal, symbol.index, stmt->start_line);
                    ADD_LOCAL_NAME(compiler, symbol.name);
                    break;
                }
                default: {
                    unreachable();
                    break;
                }
            }
            break;
        }
        case DaiAstType_AssignStatement: {
            DaiAstAssignStatement* stmt = (DaiAstAssignStatement*)node;
            DaiCompileError* err        = DaiCompiler_compile(compiler, (DaiAstBase*)stmt->value);
            if (err != NULL) {
                return err;
            }
            switch (stmt->left->type) {
                case DaiAstType_Identifier: {
                    DaiAstIdentifier* ident = (DaiAstIdentifier*)stmt->left;
                    DaiSymbol symbol;
                    bool found =
                        DaiSymbolTable_resolve(compiler->symbolTable, ident->value, &symbol);
                    if (!found || !symbol.defined) {
                        return DaiCompileError_Newf(compiler->filename,
                                                    ident->start_line,
                                                    ident->start_column,
                                                    "undefined variable: '%s'",
                                                    ident->value);
                    }
                    switch (symbol.type) {
                        case DaiSymbolType_global: {
                            DaiCompiler_emit2(
                                compiler, DaiOpSetGlobal, symbol.index, stmt->start_line);
                            ADD_GLOBAL_NAME(compiler, symbol.name);

                            break;
                        }
                        case DaiSymbolType_local: {
                            DaiCompiler_emit1(
                                compiler, DaiOpSetLocal, symbol.index, stmt->start_line);
                            ADD_LOCAL_NAME(compiler, symbol.name);
                            break;
                        }

                        default: {
                            unreachable();
                            break;
                        }
                    }

                    break;
                }
                case DaiAstType_DotExpression: {
                    DaiAstDotExpression* expr = (DaiAstDotExpression*)stmt->left;
                    DaiCompileError* err = DaiCompiler_compile(compiler, (DaiAstBase*)expr->left);
                    if (err != NULL) {
                        return err;
                    }
                    DaiObjString* name =
                        dai_copy_string(compiler->vm, expr->name->value, strlen(expr->name->value));
                    int index = DaiChunk_addConstant(&compiler->function->chunk, OBJ_VAL(name));
                    DaiCompiler_emit2(compiler, DaiOpSetProperty, index, stmt->start_line);
                    break;
                }
                case DaiAstType_SelfExpression: {
                    DaiAstSelfExpression* expr = (DaiAstSelfExpression*)stmt->left;
                    DaiObjString* name =
                        dai_copy_string(compiler->vm, expr->name->value, strlen(expr->name->value));
                    int index = DaiChunk_addConstant(&compiler->function->chunk, OBJ_VAL(name));
                    DaiCompiler_emit2(compiler, DaiOpSetSelfProperty, index, stmt->start_line);
                    break;
                }
                default: {
                    unreachable();
                    break;
                }
            }
            break;
        }
        case DaiAstType_ReturnStatement: {
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
                DaiCompileError* err =
                    DaiCompiler_compile(compiler, (DaiAstBase*)stmt->return_value);
                if (err != NULL) {
                    return err;
                }
                DaiCompiler_emit(compiler, DaiOpReturnValue, stmt->start_line);
            }
            break;
        }
        case DaiAstType_FunctionStatement: {
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
            DaiSymbol symbol = DaiSymbolTable_define(compiler->symbolTable, stmt->name);
            switch (symbol.type) {
                case DaiSymbolType_global: {
                    DaiCompiler_emit2(compiler, DaiOpDefineGlobal, symbol.index, stmt->start_line);
                    ADD_GLOBAL_NAME(compiler, symbol.name);
                    break;
                }
                case DaiSymbolType_local: {
                    if (symbol.index >= UINT8_MAX) {
                        return DaiCompileError_Newf(compiler->filename,
                                                    stmt->start_line,
                                                    stmt->start_column,
                                                    "too many local symbols(MAX=%d)",
                                                    UINT8_MAX);
                    }
                    DaiCompiler_emit1(compiler, DaiOpSetLocal, symbol.index, stmt->start_line);
                    ADD_LOCAL_NAME(compiler, symbol.name);
                    break;
                }
                default: {
                    unreachable();
                    break;
                }
            }
            IntArray_pop(&compiler->scope_stack);
            break;
        }
        case DaiAstType_ClassStatement: {
            IntArray_push(&compiler->scope_stack, ScopeType_class);
            DaiAstClassStatement* stmt = (DaiAstClassStatement*)node;
            if (DaiSymbolTable_isDefined(compiler->symbolTable, stmt->name)) {
                return DaiCompileError_Newf(compiler->filename,
                                            stmt->start_line,
                                            stmt->start_column,
                                            "symbol '%s' already defined",
                                            stmt->name);
            }
            DaiObjString* name = dai_copy_string(compiler->vm, stmt->name, strlen(stmt->name));
            int index          = DaiCompiler_addConstant(compiler, OBJ_VAL(name));
            DaiCompiler_emit2(compiler, DaiOpClass, index, stmt->start_line);
            if (stmt->parent != NULL) {
                DaiCompileError* err = DaiCompiler_compile(compiler, (DaiAstBase*)stmt->parent);
                if (err != NULL) {
                    return err;
                }
                DaiCompiler_emit(compiler, DaiOpInherit, stmt->start_line);
            }
            DaiSymbol symbol = DaiSymbolTable_define(compiler->symbolTable, stmt->name);
            switch (symbol.type) {
                case DaiSymbolType_global: {
                    DaiCompiler_emit2(compiler, DaiOpDefineGlobal, symbol.index, stmt->start_line);
                    ADD_GLOBAL_NAME(compiler, symbol.name);
                    break;
                }
                case DaiSymbolType_local: {
                    if (symbol.index >= UINT8_MAX) {
                        return DaiCompileError_Newf(compiler->filename,
                                                    stmt->start_line,
                                                    stmt->start_column,
                                                    "too many local symbols(MAX=%d)",
                                                    UINT8_MAX);
                    }
                    DaiCompiler_emit1(compiler, DaiOpSetLocal, symbol.index, stmt->start_line);
                    ADD_LOCAL_NAME(compiler, symbol.name);
                    break;
                }
                default: {
                    unreachable();
                    break;
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
            break;
        }
        case DaiAstType_InsVarStatement: {
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
                dai_copy_string(compiler->vm, stmt->name->value, strlen(stmt->name->value));
            if (DaiTable_has(&compiler->propertys, name)) {
                return DaiCompileError_Newf(compiler->filename,
                                            stmt->name->start_line,
                                            stmt->name->start_column,
                                            "property '%s' already defined",
                                            stmt->name->value);
            }
            DaiTable_set(&compiler->propertys, name, INTEGER_VAL(0));
            int index = DaiChunk_addConstant(&compiler->function->chunk, OBJ_VAL(name));
            DaiCompiler_emit2(compiler, DaiOpDefineField, index, stmt->name->start_line);
            break;
        }
        case DaiAstType_MethodStatement: {
            IntArray_push(&compiler->scope_stack, ScopeType_method);
            DaiAstMethodStatement* stmt = (DaiAstMethodStatement*)node;
            DaiObjString* name = dai_copy_string(compiler->vm, stmt->name, strlen(stmt->name));
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
            int index = DaiChunk_addConstant(&compiler->function->chunk, OBJ_VAL(name));
            DaiCompiler_emit2(compiler, DaiOpDefineMethod, index, stmt->start_line);
            IntArray_pop(&compiler->scope_stack);
            break;
        }
        case DaiAstType_ClassVarStatement: {
            DaiAstClassVarStatement* stmt = (DaiAstClassVarStatement*)node;
            DaiCompileError* err          = DaiCompiler_compile(compiler, (DaiAstBase*)stmt->value);
            if (err != NULL) {
                return err;
            }
            DaiObjString* name =
                dai_copy_string(compiler->vm, stmt->name->value, strlen(stmt->name->value));
            if (DaiTable_has(&compiler->propertys, name)) {
                return DaiCompileError_Newf(compiler->filename,
                                            stmt->start_line,
                                            stmt->start_column,
                                            "property '%s' already defined",
                                            stmt->name->value);
            }
            DaiTable_set(&compiler->propertys, name, INTEGER_VAL(0));
            int index = DaiChunk_addConstant(&compiler->function->chunk, OBJ_VAL(name));
            DaiCompiler_emit2(compiler, DaiOpDefineClassField, index, stmt->start_line);
            break;
        }
        case DaiAstType_ClassMethodStatement: {
            IntArray_push(&compiler->scope_stack, ScopeType_method);
            DaiAstClassMethodStatement* stmt = (DaiAstClassMethodStatement*)node;
            DaiCompileError* err             = DaiCompiler_compileFunction(compiler, node);
            if (err != NULL) {
                return err;
            }
            DaiObjString* name = dai_copy_string(compiler->vm, stmt->name, strlen(stmt->name));
            int index          = DaiChunk_addConstant(&compiler->function->chunk, OBJ_VAL(name));
            DaiCompiler_emit2(compiler, DaiOpDefineClassMethod, index, stmt->start_line);
            IntArray_pop(&compiler->scope_stack);
            break;
        }
        case DaiAstType_WhileStatement: {
            IntArray_push(&compiler->scope_stack, ScopeType_while);
            IntArray_enter(&compiler->break_array);
            IntArray_enter(&compiler->continue_array);
            int while_start            = compiler->function->chunk.count;
            DaiAstWhileStatement* stmt = (DaiAstWhileStatement*)node;
            DaiCompileError* err = DaiCompiler_compile(compiler, (DaiAstBase*)stmt->condition);
            if (err != NULL) {
                return err;
            }
            // 先发出虚假偏移量的跳转指令
            int jump_if_false_offset =
                DaiCompiler_emit2(compiler, DaiOpJumpIfFalse, 9999, stmt->start_line);
            err = DaiCompiler_compile(compiler, (DaiAstBase*)stmt->body);
            if (err != NULL) {
                return err;
            }
            {
                int jump_back = DaiCompiler_emit2(compiler, DaiOpJumpBack, 9999, stmt->start_line);
                DaiCompiler_patchJumpBack(compiler, jump_back, while_start);
            }
            DaiCompiler_patchJump(compiler, jump_if_false_offset);
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
                    DaiCompiler_patchJumpBack(compiler, jump, while_start);
                }
            }
            IntArray_leave(&compiler->break_array);
            IntArray_leave(&compiler->continue_array);
            IntArray_pop(&compiler->scope_stack);
            break;
        }
        case DaiAstType_BreakStatement: {
            DaiAstBreakStatement* stmt = (DaiAstBreakStatement*)node;
            if (!IntArray_contains(&compiler->scope_stack, ScopeType_while)) {
                return DaiCompileError_Newf(compiler->filename,
                                            stmt->start_line,
                                            stmt->start_column,
                                            "cannot use 'break' outside of a while loop");
            }
            int jump = DaiCompiler_emit2(compiler, DaiOpJump, 9999, stmt->start_line);
            IntArray_push(&compiler->break_array, jump);
            break;
        }
        case DaiAstType_ContinueStatement: {
            DaiAstContinueStatement* stmt = (DaiAstContinueStatement*)node;
            if (!IntArray_contains(&compiler->scope_stack, ScopeType_while)) {
                return DaiCompileError_Newf(compiler->filename,
                                            stmt->start_line,
                                            stmt->start_column,
                                            "cannot use 'continue' outside of a while loop");
            }
            int jump = DaiCompiler_emit2(compiler, DaiOpJumpBack, 9999, stmt->start_line);
            IntArray_push(&compiler->continue_array, jump);
            break;
        }
        case DaiAstType_SelfExpression: {
            DaiAstSelfExpression* expr = (DaiAstSelfExpression*)node;
            if (compiler->type != FunctionType_method &&
                compiler->type != FunctionType_classMethod) {
                return DaiCompileError_Newf(compiler->filename,
                                            expr->start_line,
                                            expr->start_column,
                                            "cannot use 'self' outside of a method");
            }
            if (expr->name == NULL) {
                DaiCompiler_emit1(compiler, DaiOpGetLocal, 0, expr->start_line);
            } else {
                DaiObjString* name =
                    dai_copy_string(compiler->vm, expr->name->value, strlen(expr->name->value));
                int index = DaiChunk_addConstant(&compiler->function->chunk, OBJ_VAL(name));
                DaiCompiler_emit2(compiler, DaiOpGetSelfProperty, index, expr->start_line);
            }
            break;
        }
        case DaiAstType_SuperExpression: {
            DaiAstSuperExpression* expr = (DaiAstSuperExpression*)node;
            if (compiler->type != FunctionType_method &&
                compiler->type != FunctionType_classMethod) {
                return DaiCompileError_Newf(compiler->filename,
                                            expr->start_line,
                                            expr->start_column,
                                            "cannot use 'super' outside of a method");
            }
            DaiObjString* name =
                dai_copy_string(compiler->vm, expr->name->value, strlen(expr->name->value));
            int index = DaiChunk_addConstant(&compiler->function->chunk, OBJ_VAL(name));
            DaiCompiler_emit2(compiler, DaiOpGetSuperProperty, index, expr->start_line);
            break;
        }
        case DaiAstType_DotExpression: {
            DaiAstDotExpression* expr = (DaiAstDotExpression*)node;
            DaiCompileError* err      = DaiCompiler_compile(compiler, (DaiAstBase*)expr->left);
            if (err != NULL) {
                return err;
            }
            DaiObjString* name =
                dai_copy_string(compiler->vm, expr->name->value, strlen(expr->name->value));
            int index = DaiChunk_addConstant(&compiler->function->chunk, OBJ_VAL(name));
            DaiCompiler_emit2(compiler, DaiOpGetProperty, index, expr->start_line);
            break;
        }
        case DaiAstType_ExpressionStatement: {
            DaiAstExpressionStatement* stmt = (DaiAstExpressionStatement*)node;
            DaiCompileError* err = DaiCompiler_compile(compiler, (DaiAstBase*)stmt->expression);
            if (err != NULL) {
                return err;
            }
            DaiCompiler_emit(compiler, DaiOpPop, stmt->start_line);
            break;
        }
        case DaiAstType_InfixExpression: {
            DaiAstInfixExpression* expr = (DaiAstInfixExpression*)node;
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
            } else if (strcmp(expr->operator, "==") == 0) {
                DaiCompiler_emit(compiler, DaiOpEqual, expr->start_line);
            } else if (strcmp(expr->operator, "!=") == 0) {
                DaiCompiler_emit(compiler, DaiOpNotEqual, expr->start_line);
            } else if (strcmp(expr->operator, "%") == 0) {
                DaiCompiler_emit(compiler, DaiOpMod, expr->start_line);
            } else {
                return DaiCompileError_Newf(compiler->filename,
                                            expr->left->end_line,
                                            expr->left->end_column + 1,
                                            "unknown operator: '%s'",
                                            expr->operator);
            }
            break;
        }
        case DaiAstType_PrefixExpression: {
            DaiAstPrefixExpression* expr = (DaiAstPrefixExpression*)node;
            DaiCompileError* err         = DaiCompiler_compile(compiler, (DaiAstBase*)expr->right);
            if (err != NULL) {
                return err;
            }
            if (strcmp(expr->operator, "-") == 0) {
                DaiCompiler_emit(compiler, DaiOpMinus, expr->start_line);
            } else if (strcmp(expr->operator, "!") == 0) {
                DaiCompiler_emit(compiler, DaiOpBang, expr->start_line);
            } else {
                return DaiCompileError_Newf(compiler->filename,
                                            expr->start_line,
                                            expr->start_column,
                                            "unknown operator: '%s'",
                                            expr->operator);
            }
            return NULL;
        }
        case DaiAstType_CallExpression: {
            DaiAstCallExpression* expr = (DaiAstCallExpression*)node;
            if (expr->arguments_count > UINT8_MAX) {
                return DaiCompileError_Newf(compiler->filename,
                                            expr->start_line,
                                            expr->start_column,
                                            "too many arguments to call, got %zu",
                                            expr->arguments_count);
            }
            switch (expr->function->type) {
                case DaiAstType_DotExpression: {
                    DaiAstDotExpression* dot = (DaiAstDotExpression*)expr->function;
                    DaiCompileError* err = DaiCompiler_compile(compiler, (DaiAstBase*)dot->left);
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
                        dai_copy_string(compiler->vm, dot->name->value, strlen(dot->name->value));
                    int index = DaiChunk_addConstant(&compiler->function->chunk, OBJ_VAL(name));
                    DaiCompiler_emit3(
                        compiler, DaiOpCallMethod, index, expr->arguments_count, expr->start_line);
                    return NULL;
                }
                case DaiAstType_SelfExpression: {
                    DaiCompiler_emit(compiler, DaiOpNil, expr->start_line);
                    for (int i = 0; i < expr->arguments_count; i++) {
                        DaiCompileError* err =
                            DaiCompiler_compile(compiler, (DaiAstBase*)expr->arguments[i]);
                        if (err != NULL) {
                            return err;
                        }
                    }
                    DaiAstSelfExpression* selfexpr = (DaiAstSelfExpression*)expr->function;
                    DaiObjString* name             = dai_copy_string(
                        compiler->vm, selfexpr->name->value, strlen(selfexpr->name->value));
                    int index = DaiChunk_addConstant(&compiler->function->chunk, OBJ_VAL(name));
                    DaiCompiler_emit3(compiler,
                                      DaiOpCallSelfMethod,
                                      index,
                                      expr->arguments_count,
                                      expr->start_line);
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
                    DaiObjString* name              = dai_copy_string(
                        compiler->vm, superexpr->name->value, strlen(superexpr->name->value));
                    int index = DaiChunk_addConstant(&compiler->function->chunk, OBJ_VAL(name));
                    DaiCompiler_emit3(compiler,
                                      DaiOpCallSuperMethod,
                                      index,
                                      expr->arguments_count,
                                      expr->start_line);
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
            break;
        }
        case DaiAstType_Identifier: {
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
            break;
        }
        case DaiAstType_FunctionLiteral: {
            return DaiCompiler_compileFunction(compiler, node);
        }
        case DaiAstType_IntegerLiteral: {
            DaiAstIntegerLiteral* lit = (DaiAstIntegerLiteral*)node;
            DaiValue integer          = INTEGER_VAL(lit->value);
            DaiCompiler_emit2(compiler,
                              DaiOpConstant,
                              DaiCompiler_addConstant(compiler, integer),
                              lit->start_line);
            break;
        }
        case DaiAstType_FloatLiteral: {
            DaiAstFloatLiteral* lit = (DaiAstFloatLiteral*)node;
            DaiValue num            = FLOAT_VAL(lit->value);
            DaiCompiler_emit2(
                compiler, DaiOpConstant, DaiCompiler_addConstant(compiler, num), lit->start_line);
            break;
        }
        case DaiAstType_StringLiteral: {
            DaiAstStringLiteral* lit = (DaiAstStringLiteral*)node;
            // +1 -2 是为了去掉字符串前后的引号
            DaiObjString* string =
                dai_copy_string(compiler->vm, lit->value + 1, strlen(lit->value) - 2);
            DaiCompiler_emit2(compiler,
                              DaiOpConstant,
                              DaiCompiler_addConstant(compiler, OBJ_VAL(string)),
                              lit->start_line);
            break;
        }
        case DaiAstType_Boolean: {
            DaiAstBoolean* lit = (DaiAstBoolean*)node;
            if (lit->value) {
                DaiCompiler_emit(compiler, DaiOpTrue, lit->start_line);
            } else {
                DaiCompiler_emit(compiler, DaiOpFalse, lit->start_line);
            }
            break;
        }
        case DaiAstType_Nil: {
            DaiAstNil* lit = (DaiAstNil*)node;
            DaiCompiler_emit(compiler, DaiOpNil, lit->start_line);
            break;
        }
        default: {
            fprintf(stderr, "unknown node type: %s\n", DaiAstType_string(node->type));
            abort();
        }
    }
    return NULL;
}


static int
DaiCompiler_addConstant(DaiCompiler* compiler, DaiValue value) {
    DaiChunk* chunk = &(compiler->function->chunk);
    return DaiChunk_addConstant(chunk, value);
}

static int
DaiCompiler_emit(DaiCompiler* compiler, DaiOpCode op, int line) {
    DaiChunk* chunk = &(compiler->function->chunk);
    DaiChunk_write(chunk, (uint8_t)op, line);
    return chunk->count - 1;
}

static int
DaiCompiler_emit1(DaiCompiler* compiler, DaiOpCode op, uint8_t operand, int line) {
    DaiChunk* chunk = &(compiler->function->chunk);
    DaiChunk_write(chunk, (uint8_t)op, line);
    DaiChunk_write(chunk, operand, line);
    return chunk->count - 2;
}
static int
DaiCompiler_emit2(DaiCompiler* compiler, DaiOpCode op, uint16_t operand, int line) {
    DaiChunk* chunk = &(compiler->function->chunk);
    DaiChunk_writeu16(chunk, op, operand, line);
    return chunk->count - 3;
}
static int
DaiCompiler_emit3(DaiCompiler* compiler, DaiOpCode op, uint16_t operand1, uint8_t operand2,
                  int line) {
    DaiChunk* chunk = &(compiler->function->chunk);
    DaiChunk_writeu16(chunk, op, operand1, line);
    DaiChunk_write(chunk, operand2, line);
    return chunk->count - 4;
}

static void
DaiCompiler_patchJump(DaiCompiler* compiler, int offset) {
    DaiChunk* chunk = &(compiler->function->chunk);
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
DaiCompiler_patchJumpBack(DaiCompiler* compiler, int offset, int dst) {
    DaiChunk* chunk = &(compiler->function->chunk);
    // 减去3是因为 jump 指令占用3个字节
    int jump = offset - dst + 3;

    if (jump > UINT16_MAX) {
        fprintf(stderr, "too much code to jump over\n");
        abort();
    }

    chunk->code[offset + 1] = (uint8_t)((jump >> 8) & 0xff);
    chunk->code[offset + 2] = (uint8_t)(jump & 0xff);
}

DaiCompileError*
dai_compile(DaiAstProgram* program, DaiObjFunction* function, DaiVM* vm) {
    vm->state = VMState_compiling;
    DaiCompiler compiler;
    DaiCompiler_init(&compiler, function, vm->globalSymbolTable, FunctionType_script, vm);
    IntArray_push(&compiler.scope_stack, ScopeType_script);
    DaiCompileError* err = NULL;
    err                  = DaiCompiler_extractSymbol(&compiler, (DaiAstBase*)program);
    if (err != NULL) {
        DaiCompiler_reset(&compiler);
        DaiAstProgram_reset(program);
        return err;
    }
    err = DaiCompiler_compile(&compiler, (DaiAstBase*)program);
    DaiCompiler_reset(&compiler);
    DaiAstProgram_reset(program);
    return err;
}
