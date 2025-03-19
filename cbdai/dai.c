#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dai.h"
#include "dai_compile.h"
#include "dai_object.h"
#include "dai_parse.h"
#include "dai_symboltable.h"
#include "dai_tokenize.h"
#include "dai_utils.h"
#include "dai_value.h"
#include "dai_vm.h"

typedef struct Dai {
    DaiVM vm;
    bool loaded;

    // for call dai script function
    DaiObj* function;
    int argc;
    DaiValue ret;

    // for call c function
    DaiValue* argv;
    int pop_arg_index;

} Dai;

Dai*
dai_new() {
    Dai* dai = malloc(sizeof(Dai));
    if (dai == NULL) {
        perror("malloc error");
        abort();
    }
    DaiVM_init(&dai->vm);
    dai->loaded = false;

    dai->function = NULL;
    dai->argc     = 0;
    dai->ret      = UNDEFINED_VAL;

    dai->argv          = NULL;
    dai->pop_arg_index = 0;

    return dai;
}

void
dai_free(Dai* dai) {
    DaiVM_reset(&dai->vm);
    free(dai);
}

void
dai_load_file(Dai* dai, const char* filename) {
    if (dai->loaded) {
        fprintf(stderr, "dai_load_file can only be called once.\n");
        abort();
    }
    dai->loaded = true;
    // convert to absolute path
    char* filepath = realpath(filename, NULL);
    if (filepath == NULL) {
        perror("realpath error");
        abort();
    }
    char* text = dai_string_from_file(filepath);
    if (text == NULL) {
        perror("open file error");
        abort();
    }
    DaiError* err = NULL;
    DaiTokenList tlist;
    DaiTokenList_init(&tlist);
    DaiAstProgram program;
    DaiAstProgram_init(&program);

    err = dai_tokenize_string(text, &tlist);
    if (err != NULL) {
        DaiSyntaxError_setFilename(err, filename);
        DaiSyntaxError_pprint(err, text);
        abort();
    }
    err = dai_parse(&tlist, &program);
    if (err != NULL) {
        DaiSyntaxError_setFilename(err, filename);
        DaiSyntaxError_pprint(err, text);
        abort();
    }
    DaiObjFunction* function = DaiObjFunction_New(&dai->vm, "<main>", filepath);
    err                      = dai_compile(&program, function, &dai->vm);
    if (err != NULL) {
        DaiCompileError_pprint(err, text);
        abort();
    }
    DaiObjError* runtime_err = DaiVM_run(&dai->vm, function);
    if (runtime_err != NULL) {
        DaiVM_printError(&dai->vm, runtime_err);
        abort();
    }
    free(filepath);
    free(text);
    DaiTokenList_reset(&tlist);
    DaiAstProgram_reset(&program);
}

int64_t
dai_get_int(Dai* dai, const char* name) {
    DaiValue value;
    if (!DaiVM_getGlobal(&dai->vm, name, &value)) {
        fprintf(stderr, "dai_get_int: variable '%s' not found.\n", name);
        abort();
    }
    if (!IS_INTEGER(value)) {
        fprintf(stderr,
                "dai_get_int: variable '%s' expected int, but got %s.\n",
                name,
                dai_value_ts(value));
        abort();
    }
    return AS_INTEGER(value);
}

void
dai_set_int(Dai* dai, const char* name, int64_t value) {
    if (!DaiVM_setGlobal(&dai->vm, name, INTEGER_VAL(value))) {
        fprintf(stderr, "dai_set_int: variable '%s' not found.\n", name);
        abort();
    }
}

double
dai_get_float(Dai* dai, const char* name) {
    DaiValue value;
    if (!DaiVM_getGlobal(&dai->vm, name, &value)) {
        fprintf(stderr, "dai_get_float: variable '%s' not found.\n", name);
        abort();
    }
    if (!IS_FLOAT(value)) {
        fprintf(stderr,
                "dai_get_float: variable '%s' expected float, but got %s.\n",
                name,
                dai_value_ts(value));
        abort();
    }
    return AS_FLOAT(value);
}

void
dai_set_float(Dai* dai, const char* name, double value) {
    if (!DaiVM_setGlobal(&dai->vm, name, FLOAT_VAL(value))) {
        fprintf(stderr, "dai_set_float: variable '%s' not found.\n", name);
        abort();
    }
}

const char*
dai_get_string(Dai* dai, const char* name) {
    DaiValue value;
    if (!DaiVM_getGlobal(&dai->vm, name, &value)) {
        fprintf(stderr, "dai_get_string: variable '%s' not found.\n", name);
        abort();
    }
    if (!IS_STRING(value)) {
        fprintf(stderr,
                "dai_get_string: variable '%s' expected string, but got %s.\n",
                name,
                dai_value_ts(value));
        abort();
    }
    return AS_CSTRING(value);
}

void
dai_set_string(Dai* dai, const char* name, const char* value) {
    DaiValue v = OBJ_VAL(dai_copy_string(&dai->vm, value, strlen(value)));
    if (!DaiVM_setGlobal(&dai->vm, name, v)) {
        fprintf(stderr, "dai_set_string: variable '%s' not found.\n", name);
        abort();
    }
}

dai_func_t
dai_get_function(Dai* dai, const char* name) {
    DaiValue value;
    if (!DaiVM_getGlobal(&dai->vm, name, &value)) {
        fprintf(stderr, "dai_get_function: variable '%s' not found.\n", name);
        abort();
    }
    if (!IS_FUNCTION(value) && !IS_CLOSURE(value)) {
        fprintf(stderr,
                "dai_get_function: variable '%s' expected function, but got %s.\n",
                name,
                dai_value_ts(value));
        abort();
    }
    return (dai_func_t)AS_OBJ(value);
}


// #region Call function in dai script.
void
dai_call_push_function(Dai* dai, dai_func_t func) {
    dai->function = (DaiObj*)func;
    DaiVM_push1(&dai->vm, OBJ_VAL((DaiObjFunction*)func));
}

// push argument to function call
void
dai_call_push_arg_int(Dai* dai, int64_t value) {
    DaiVM_push1(&dai->vm, INTEGER_VAL(value));
    dai->argc++;
}
void
dai_call_push_arg_float(Dai* dai, double value) {
    DaiVM_push1(&dai->vm, FLOAT_VAL(value));
    dai->argc++;
}
void
dai_call_push_arg_string(Dai* dai, const char* value) {
    DaiValue v = OBJ_VAL(dai_copy_string(&dai->vm, value, strlen(value)));
    DaiVM_push1(&dai->vm, v);
    dai->argc++;
}
void
dai_call_push_arg_function(Dai* dai, dai_func_t value) {
    DaiValue v = OBJ_VAL((DaiObjFunction*)value);
    DaiVM_push1(&dai->vm, v);
    dai->argc++;
}
void
dai_call_push_arg_nil(Dai* dai) {
    DaiVM_push1(&dai->vm, NIL_VAL);
    dai->argc++;
}

void
dai_call_execute(Dai* dai) {
    if (dai->function == NULL) {
        fprintf(stderr, "dai_call_execute: function not set.\n");
        abort();
    }
    DaiValue ret = DaiVM_runCall2(&dai->vm, OBJ_VAL(dai->function), dai->argc);
    if (IS_ERROR(ret)) {
        DaiVM_printError(&dai->vm, AS_ERROR(ret));
        abort();
    }
    dai->argc     = 0;
    dai->function = NULL;
    dai->ret      = ret;
}

int64_t
dai_call_return_int(Dai* dai) {
    if (!IS_INTEGER(dai->ret)) {
        fprintf(stderr, "dai_call_return_int: expected int, but got %s.\n", dai_value_ts(dai->ret));
        abort();
    }
    return AS_INTEGER(dai->ret);
}

double
dai_call_return_float(Dai* dai) {
    if (!IS_FLOAT(dai->ret)) {
        fprintf(
            stderr, "dai_call_return_float: expected float, but got %s.\n", dai_value_ts(dai->ret));
        abort();
    }
    return AS_FLOAT(dai->ret);
}

const char*
dai_call_return_string(Dai* dai) {
    if (!IS_STRING(dai->ret)) {
        fprintf(stderr,
                "dai_call_return_string: expected string, but got %s.\n",
                dai_value_ts(dai->ret));
        abort();
    }
    return AS_CSTRING(dai->ret);
}

dai_func_t
dai_call_return_function(Dai* dai) {
    if (!IS_FUNCTION(dai->ret) && !IS_CLOSURE(dai->ret)) {
        fprintf(stderr,
                "dai_call_return_function: expected function, but got %s.\n",
                dai_value_ts(dai->ret));
        abort();
    }
    return (dai_func_t)AS_OBJ(dai->ret);
}

// #endregion


// #region Register C function to dai script

// 包装 DaiVM 中对 DaiObjCFunction 的调用，设置一些状态，调用实际的 C 函数
static DaiValue
dai_cfunction_wrapper(DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    DaiObjCFunction* c_fn = AS_CFUNCTION(receiver);
    if (argc != c_fn->arity) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "%s() expected %d arguments, but got %d", c_fn->name, c_fn->arity, argc);
        return OBJ_VAL(err);
    }
    Dai* dai           = (Dai*)c_fn->dai;
    dai->argv          = argv;
    dai->argc          = argc;
    dai->pop_arg_index = 0;
    dai->ret           = UNDEFINED_VAL;
    c_fn->func(dai);
    if (IS_UNDEFINED(dai->ret)) {
        fprintf(stderr, "dai_cfunction_wrapper: return value not set.\n");
        abort();
    }
    DaiValue ret = dai->ret;
    dai->ret     = UNDEFINED_VAL;
    return ret;
}

void
dai_register_function(Dai* dai, const char* name, dai_c_func_t func, int arity) {
    DaiObjCFunction* c_fn =
        DaiObjCFunction_New(&dai->vm, dai, dai_cfunction_wrapper, func, name, arity);
    DaiSymbolTable_define(dai->vm.globalSymbolTable, name, true);
    DaiVM_setGlobal(&dai->vm, name, OBJ_VAL(c_fn));
}

// pop argument from DaiVM
int64_t
dai_call_pop_arg_int(Dai* dai) {
    if (dai->pop_arg_index >= dai->argc) {
        fprintf(stderr, "dai_call_pop_arg_int: no more argument.\n");
        abort();
    }
    if (!IS_INTEGER(dai->argv[dai->pop_arg_index])) {
        fprintf(stderr,
                "dai_call_pop_arg_int: expected int, but got %s.\n",
                dai_value_ts(dai->argv[dai->pop_arg_index]));
        abort();
    }
    return AS_INTEGER(dai->argv[dai->pop_arg_index++]);
}
double
dai_call_pop_arg_float(Dai* dai) {
    if (dai->pop_arg_index >= dai->argc) {
        fprintf(stderr, "dai_call_pop_arg_float: no more argument.\n");
        abort();
    }
    if (!IS_FLOAT(dai->argv[dai->pop_arg_index])) {
        fprintf(stderr,
                "dai_call_pop_arg_float: expected float, but got %s.\n",
                dai_value_ts(dai->argv[dai->pop_arg_index]));
        abort();
    }
    return AS_FLOAT(dai->argv[dai->pop_arg_index++]);
}

const char*
dai_call_pop_arg_string(Dai* dai) {
    if (dai->pop_arg_index >= dai->argc) {
        fprintf(stderr, "dai_call_pop_arg_string: no more argument.\n");
        abort();
    }
    if (!IS_STRING(dai->argv[dai->pop_arg_index])) {
        fprintf(stderr,
                "dai_call_pop_arg_string: expected string, but got %s.\n",
                dai_value_ts(dai->argv[dai->pop_arg_index]));
        abort();
    }
    return AS_CSTRING(dai->argv[dai->pop_arg_index++]);
}

void
dai_call_push_return_int(Dai* dai, int64_t value) {
    if (!IS_UNDEFINED(dai->ret)) {
        fprintf(stderr, "dai_call_push_return_int: return value already set.\n");
        abort();
    }
    dai->ret = INTEGER_VAL(value);
}

void
dai_call_push_return_float(Dai* dai, double value) {
    if (!IS_UNDEFINED(dai->ret)) {
        fprintf(stderr, "dai_call_push_return_int: return value already set.\n");
        abort();
    }
    dai->ret = FLOAT_VAL(value);
}

void
dai_call_push_return_string(Dai* dai, const char* value) {
    if (!IS_UNDEFINED(dai->ret)) {
        fprintf(stderr, "dai_call_push_return_int: return value already set.\n");
        abort();
    }
    dai->ret = OBJ_VAL(dai_copy_string(&dai->vm, value, strlen(value)));
}

void
dai_call_push_return_nil(Dai* dai) {
    if (!IS_UNDEFINED(dai->ret)) {
        fprintf(stderr, "dai_call_push_return_int: return value already set.\n");
        abort();
    }
    dai->ret = NIL_VAL;
}

// #endregion
