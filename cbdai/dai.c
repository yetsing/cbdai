#include "dai.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dai_object.h"
#include "dai_value.h"
#include "dai_vm.h"
#include "dairun.h"

typedef struct Dai {
    DaiVM vm;
    DaiObjModule* module;
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
    dai->module = DaiObjModule_New(&dai->vm, strdup("__main__"), strdup("<main>"));
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
    dai->loaded      = true;
    DaiObjError* err = Dairun_FileWithModule(&dai->vm, filename, dai->module);
    if (err != NULL) {
        DaiVM_printError(&dai->vm, err);
        abort();
    }
}

int64_t
dai_get_int(Dai* dai, const char* name) {
    DaiValue value;
    if (!DaiObjModule_get_global(dai->module, name, &value)) {
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
    if (!DaiObjModule_set_global(dai->module, name, INTEGER_VAL(value))) {
        fprintf(stderr, "dai_set_int: variable '%s' not found.\n", name);
        abort();
    }
}

double
dai_get_float(Dai* dai, const char* name) {
    DaiValue value;
    if (!DaiObjModule_get_global(dai->module, name, &value)) {
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
    if (!DaiObjModule_set_global(dai->module, name, FLOAT_VAL(value))) {
        fprintf(stderr, "dai_set_float: variable '%s' not found.\n", name);
        abort();
    }
}

const char*
dai_get_string(Dai* dai, const char* name) {
    DaiValue value;
    if (!DaiObjModule_get_global(dai->module, name, &value)) {
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
    if (!DaiObjModule_set_global(dai->module, name, v)) {
        fprintf(stderr, "dai_set_string: variable '%s' not found.\n", name);
        abort();
    }
}

dai_func_t
dai_get_function(Dai* dai, const char* name) {
    DaiValue value;
    if (!DaiObjModule_get_global(dai->module, name, &value)) {
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


// #region Call function of dai script.
void
daicall_push_function(Dai* dai, dai_func_t func) {
    dai->function = (DaiObj*)func;
    DaiVM_push1(&dai->vm, OBJ_VAL((DaiObjFunction*)func));
}

// push argument to function call
void
daicall_pusharg_int(Dai* dai, int64_t value) {
    DaiVM_push1(&dai->vm, INTEGER_VAL(value));
    dai->argc++;
}
void
daicall_pusharg_float(Dai* dai, double value) {
    DaiVM_push1(&dai->vm, FLOAT_VAL(value));
    dai->argc++;
}
void
daicall_pusharg_string(Dai* dai, const char* value) {
    DaiValue v = OBJ_VAL(dai_copy_string(&dai->vm, value, strlen(value)));
    DaiVM_push1(&dai->vm, v);
    dai->argc++;
}
void
daicall_pusharg_function(Dai* dai, dai_func_t value) {
    DaiValue v = OBJ_VAL((DaiObjFunction*)value);
    DaiVM_push1(&dai->vm, v);
    dai->argc++;
}
void
daicall_pusharg_nil(Dai* dai) {
    DaiVM_push1(&dai->vm, NIL_VAL);
    dai->argc++;
}

void
daicall_execute(Dai* dai) {
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
daicall_getrv_int(Dai* dai) {
    if (!IS_INTEGER(dai->ret)) {
        fprintf(stderr, "dai_call_return_int: expected int, but got %s.\n", dai_value_ts(dai->ret));
        abort();
    }
    return AS_INTEGER(dai->ret);
}

double
daicall_getrv_float(Dai* dai) {
    if (!IS_FLOAT(dai->ret)) {
        fprintf(
            stderr, "dai_call_return_float: expected float, but got %s.\n", dai_value_ts(dai->ret));
        abort();
    }
    return AS_FLOAT(dai->ret);
}

const char*
daicall_getrv_string(Dai* dai) {
    if (!IS_STRING(dai->ret)) {
        fprintf(stderr,
                "dai_call_return_string: expected string, but got %s.\n",
                dai_value_ts(dai->ret));
        abort();
    }
    return AS_CSTRING(dai->ret);
}

dai_func_t
daicall_getrv_function(Dai* dai) {
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
        DaiObjCFunction_New(&dai->vm, dai, dai_cfunction_wrapper, (CFunction)func, name, arity);
    DaiObjModule_add_global(dai->module, name, OBJ_VAL(c_fn));
}

// pop argument from DaiVM
int64_t
daicall_poparg_int(Dai* dai) {
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
daicall_poparg_float(Dai* dai) {
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
daicall_poparg_string(Dai* dai) {
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
daicall_setrv_int(Dai* dai, int64_t value) {
    if (!IS_UNDEFINED(dai->ret)) {
        fprintf(stderr, "dai_call_push_return_int: return value already set.\n");
        abort();
    }
    dai->ret = INTEGER_VAL(value);
}

void
daicall_setrv_float(Dai* dai, double value) {
    if (!IS_UNDEFINED(dai->ret)) {
        fprintf(stderr, "dai_call_push_return_int: return value already set.\n");
        abort();
    }
    dai->ret = FLOAT_VAL(value);
}

void
daicall_setrv_string(Dai* dai, const char* value) {
    if (!IS_UNDEFINED(dai->ret)) {
        fprintf(stderr, "dai_call_push_return_int: return value already set.\n");
        abort();
    }
    dai->ret = OBJ_VAL(dai_copy_string(&dai->vm, value, strlen(value)));
}

void
daicall_setrv_nil(Dai* dai) {
    if (!IS_UNDEFINED(dai->ret)) {
        fprintf(stderr, "dai_call_push_return_int: return value already set.\n");
        abort();
    }
    dai->ret = NIL_VAL;
}

// #endregion
