#include "dai_builtin.h"

#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cwalk.h"

#include "dai_object.h"
#include "dai_utils.h"
#include "dai_value.h"
#include "dai_vm.h"


// #region 内置函数
static DaiValue
builtin_print(__attribute__((unused)) DaiVM* vm, __attribute__((unused)) DaiValue receiver,
              int argc, DaiValue* argv) {
    for (int i = 0; i < argc; i++) {
        dai_print_value(argv[i]);
        dai_log(" ");
    }
    dai_log("\n");
    return NIL_VAL;
}

static DaiValue
builtin_len(__attribute__((unused)) DaiVM* vm, __attribute__((unused)) DaiValue receiver, int argc,
            DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err = DaiObjError_Newf(vm, "len() expected 1 argument, but got %d", argc);
        return OBJ_VAL(err);
    }
    const DaiValue arg = argv[0];
    if (IS_STRING(arg)) {
        return INTEGER_VAL(AS_STRING(arg)->length);
    } else if (IS_ARRAY(arg)) {
        return INTEGER_VAL(AS_ARRAY(arg)->length);
    } else {
        DaiObjError* err = DaiObjError_Newf(vm, "'len' not supported '%s'", dai_value_ts(arg));
        return OBJ_VAL(err);
    }
    return NIL_VAL;
}

static DaiValue
builtin_type(__attribute__((unused)) DaiVM* vm, __attribute__((unused)) DaiValue receiver, int argc,
             DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err = DaiObjError_Newf(vm, "type() expected 1 argument, but got %d", argc);
        return OBJ_VAL(err);
    }
    const DaiValue arg = argv[0];
    const char* s      = dai_value_ts(arg);
    return OBJ_VAL(dai_copy_string_intern(vm, s, strlen(s)));
}

static DaiValue
builtin_assert(__attribute__((unused)) DaiVM* vm, __attribute__((unused)) DaiValue receiver,
               int argc, DaiValue* argv) {
    if ((argc != 1) && (argc != 2)) {
        DaiObjError* err = DaiObjError_Newf(vm, "assert() expected 2 argument, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (argc == 2 && !IS_STRING(argv[1])) {
        DaiObjError* err = DaiObjError_Newf(vm, "assert() expected string as second argument");
        return OBJ_VAL(err);
    }
    if (!dai_value_is_truthy(argv[0])) {
        if (argc == 1) {
            DaiObjError* err = DaiObjError_Newf(vm, "assertion failed");
            return OBJ_VAL(err);
        }
        DaiObjError* err = DaiObjError_Newf(vm, "assertion failed: %s", AS_STRING(argv[1])->chars);
        return OBJ_VAL(err);
    }
    return NIL_VAL;
}

static DaiValue
builtin_assert_eq(__attribute__((unused)) DaiVM* vm, __attribute__((unused)) DaiValue receiver,
                  int argc, DaiValue* argv) {
    if ((argc != 2) && (argc != 3)) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "assert_eq() expected 2 or 3 argument, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (argc == 3 && !IS_STRING(argv[2])) {
        DaiObjError* err = DaiObjError_Newf(vm, "assert_eq() expected string as third argument");
        return OBJ_VAL(err);
    }
    if (!dai_value_equal(argv[0], argv[1])) {
        char* s1         = dai_value_string(argv[0]);
        char* s2         = dai_value_string(argv[1]);
        DaiObjError* err = NULL;
        if (argc == 2) {
            err = DaiObjError_Newf(vm, "assertion failed: %s != %s", s1, s2);
        } else {
            err = DaiObjError_Newf(
                vm, "assertion failed: %s != %s %s", s1, s2, AS_STRING(argv[2])->chars);
        }
        free(s1);
        free(s2);
        return OBJ_VAL(err);
    }
    return NIL_VAL;
}

static DaiValue
builtin_range(__attribute__((unused)) DaiVM* vm, __attribute__((unused)) DaiValue receiver,
              int argc, DaiValue* argv) {
    if (argc == 0 || argc > 3) {
        DaiObjError* err = DaiObjError_Newf(vm, "range() expected 1-3 argument, but got %d", argc);
        return OBJ_VAL(err);
    }
    for (int i = 0; i < argc; ++i) {
        if (!IS_INTEGER(argv[i])) {
            DaiObjError* err = DaiObjError_Newf(
                vm, "range() expected int arguments, but got %s", dai_value_ts(argv[0]));
            return OBJ_VAL(err);
        }
    }
    int64_t start = 0, end = 0, step = 1;
    if (argc == 1) {
        end = AS_INTEGER(argv[0]);
    } else if (argc == 2) {
        start = AS_INTEGER(argv[0]);
        end   = AS_INTEGER(argv[1]);
    } else {
        start = AS_INTEGER(argv[0]);
        end   = AS_INTEGER(argv[1]);
        step  = AS_INTEGER(argv[2]);
    }
    DaiObjRangeIterator* iterator = DaiObjRangeIterator_New(vm, start, end, step);
    return OBJ_VAL(iterator);
}

static DaiValue
builtin_abs(__attribute__((unused)) DaiVM* vm, __attribute__((unused)) DaiValue receiver, int argc,
            DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err = DaiObjError_Newf(vm, "abs() expected 1 argument, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (IS_INTEGER(argv[0])) {
        return INTEGER_VAL(llabs(AS_INTEGER(argv[0])));
    } else if (IS_FLOAT(argv[0])) {
        return FLOAT_VAL(fabs(AS_FLOAT(argv[0])));
    } else {
        DaiObjError* err = DaiObjError_Newf(
            vm, "abs() expected number arguments, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
}

static DaiValue
builtin_import(DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
#define SUFFIX_LEN 4   // .dai 的长度
    if (argc != 1) {
        DaiObjError* err = DaiObjError_Newf(vm, "import() expected 1 argument, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (!IS_STRING(argv[0])) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "import() expected string argument, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
    char abs_path[PATH_MAX];
    const char* path             = AS_STRING(argv[0])->chars;
    const char* current_filename = DaiVM_getCurrentFilename(vm);
    size_t length;
    cwk_path_get_dirname(current_filename, &length);
    const char* current_dir = strndup(current_filename, length);
    assert(current_dir != NULL);
    cwk_path_get_absolute(current_dir, path, abs_path, sizeof(abs_path));
    free((void*)current_dir);

    DaiObjModule* module = DaiVM_getModule(vm, abs_path);
    if (module != NULL) {
        return OBJ_VAL(module);
    }
    const char* text = dai_string_from_file(abs_path);
    if (text == NULL) {
        DaiObjError* err = DaiObjError_Newf(vm, "import() failed to read file: %s", abs_path);
        return OBJ_VAL(err);
    }
    const char* basename;
    cwk_path_get_basename(abs_path, &basename, &length);
    DaiVM_pauseGC(vm);
    module = DaiObjModule_New(vm, strndup(basename, length - SUFFIX_LEN), strdup(abs_path));
    DaiObjError* err = DaiVM_loadModule(vm, text, module);
    free((void*)text);
    if (err != NULL) {
        return OBJ_VAL(err);
    }
    return OBJ_VAL(module);
}

static DaiObjBuiltinFunction builtin_funcs[] = {
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "print",
        .function = builtin_print,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "len",
        .function = builtin_len,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "type",
        .function = builtin_type,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "assert",
        .function = builtin_assert,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "assert_eq",
        .function = builtin_assert_eq,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "range",
        .function = builtin_range,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "abs",
        .function = builtin_abs,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "import",
        .function = builtin_import,
    },

    {
        .name = NULL,
    },
};

// #endregion

// #region 内置模块 time

static DaiValue
builtin_time_time(__attribute__((unused)) DaiVM* vm, __attribute__((unused)) DaiValue receiver,
                  int argc, DaiValue* argv) {
    if (argc != 0) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "time.time() expected no arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return INTEGER_VAL(ts.tv_sec);
}

static DaiValue
builtin_time_timef(__attribute__((unused)) DaiVM* vm, __attribute__((unused)) DaiValue receiver,
                   int argc, DaiValue* argv) {
    if (argc != 0) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "time.timef() expected no arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return FLOAT_VAL(ts.tv_sec + (double)ts.tv_nsec / 1e9);
}


static DaiValue
builtin_time_sleep(__attribute__((unused)) DaiVM* vm, __attribute__((unused)) DaiValue receiver,
                   int argc, DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "time.sleep() expected 1 argument, but got %d", argc);
        return OBJ_VAL(err);
    }
    DaiValue val = argv[0];
    struct timespec req;
    if (IS_INTEGER(val)) {
        req.tv_sec  = AS_INTEGER(val);
        req.tv_nsec = 0;
    } else if (IS_FLOAT(val)) {
        req.tv_sec  = (time_t)AS_FLOAT(val);
        req.tv_nsec = (long)((AS_FLOAT(val) - (double)req.tv_sec) * 1e9);
    } else {
        DaiObjError* err = DaiObjError_Newf(
            vm, "time.sleep() expected number arguments, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
    nanosleep(&req, NULL);
    return NIL_VAL;
}

static DaiObjBuiltinFunction builtin_time_funcs[] = {
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "time",
        .function = builtin_time_time,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "timef",
        .function = builtin_time_timef,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "sleep",
        .function = builtin_time_sleep,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name = NULL,
    },
};

DaiObjModule*
builtin_time_module(DaiVM* vm) {
    DaiObjModule* module = DaiObjModule_New(vm, strdup("time"), strdup("<builtin>"));
    for (int i = 0; builtin_time_funcs[i].name != NULL; i++) {
        DaiObjModule_addGlobal(module, builtin_time_funcs[i].name, OBJ_VAL(&builtin_time_funcs[i]));
    }
    return module;
}

// #endregion

// #region 内置模块 math

static DaiValue
builtin_math_sqrt(__attribute__((unused)) DaiVM* vm, __attribute__((unused)) DaiValue receiver,
                  int argc, DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "math.sqrt() expected 1 argument, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (IS_INTEGER(argv[0])) {
        double n = sqrt(AS_INTEGER(argv[0]));
        return FLOAT_VAL(n);
    } else if (IS_FLOAT(argv[0])) {
        double n = sqrt(AS_FLOAT(argv[0]));
        return FLOAT_VAL(n);
    } else {
        DaiObjError* err = DaiObjError_Newf(
            vm, "math.sqrt() expected number arguments, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
}

static DaiValue
builtin_math_sin(__attribute__((unused)) DaiVM* vm, __attribute__((unused)) DaiValue receiver,
                 int argc, DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err = DaiObjError_Newf(vm, "math.sin() expected 1 argument, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (IS_INTEGER(argv[0])) {
        double n = sin(AS_INTEGER(argv[0]));
        return FLOAT_VAL(n);
    } else if (IS_FLOAT(argv[0])) {
        double n = sin(AS_FLOAT(argv[0]));
        return FLOAT_VAL(n);
    } else {
        DaiObjError* err = DaiObjError_Newf(
            vm, "math.sin() expected number arguments, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
}

static DaiValue
builtin_math_cos(__attribute__((unused)) DaiVM* vm, __attribute__((unused)) DaiValue receiver,
                 int argc, DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err = DaiObjError_Newf(vm, "math.cos() expected 1 argument, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (IS_INTEGER(argv[0])) {
        double n = cos(AS_INTEGER(argv[0]));
        return FLOAT_VAL(n);
    } else if (IS_FLOAT(argv[0])) {
        double n = cos(AS_FLOAT(argv[0]));
        return FLOAT_VAL(n);
    } else {
        DaiObjError* err = DaiObjError_Newf(
            vm, "math.cos() expected number arguments, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
}

static DaiObjBuiltinFunction builtin_math_funcs[] = {
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "sqrt",
        .function = builtin_math_sqrt,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "sin",
        .function = builtin_math_sin,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "cos",
        .function = builtin_math_cos,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name = NULL,
    },
};
DaiObjModule*
builtin_math_module(DaiVM* vm) {
    DaiObjModule* module = DaiObjModule_New(vm, strdup("math"), strdup("<builtin>"));
    for (int i = 0; builtin_math_funcs[i].name != NULL; i++) {
        DaiObjModule_addGlobal(module, builtin_math_funcs[i].name, OBJ_VAL(&builtin_math_funcs[i]));
    }
    return module;
}

// #endregion

const char* builtin_names[BUILTIN_OBJECT_MAX_COUNT]               = {};
static DaiBuiltinObject builtin_objects[BUILTIN_OBJECT_MAX_COUNT] = {};

DaiBuiltinObject*
init_builtin_objects(DaiVM* vm, int* count) {
    int i = 0;
    for (; builtin_funcs[i].name != NULL; i++) {
        builtin_names[i]         = builtin_funcs[i].name;
        builtin_objects[i].name  = builtin_funcs[i].name;
        builtin_objects[i].value = OBJ_VAL(&builtin_funcs[i]);
    }
    builtin_names[i]         = "time";
    builtin_objects[i].name  = "time";
    builtin_objects[i].value = OBJ_VAL(builtin_time_module(vm));
    i++;

    builtin_names[i]         = "math";
    builtin_objects[i].name  = "math";
    builtin_objects[i].value = OBJ_VAL(builtin_math_module(vm));
    i++;

    *count = i;
    return builtin_objects;
}
