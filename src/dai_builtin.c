#include "dai_builtin.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

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
    // loadModule 会恢复 GC ，所以不需要手动恢复
    DaiObjError* err = DaiVM_loadModule(vm, text, module);
    free((void*)text);
    if (err != NULL) {
        return OBJ_VAL(err);
    }
    return OBJ_VAL(module);
}

static DaiValue
builtin_int(DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err = DaiObjError_Newf(vm, "int() expected 1 argument, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (IS_INTEGER(argv[0])) {
        return argv[0];
    } else if (IS_FLOAT(argv[0])) {
        return INTEGER_VAL((int64_t)AS_FLOAT(argv[0]));
    } else if (IS_STRING(argv[0])) {
        char* endptr;
        errno    = 0;
        long val = strtol(AS_STRING(argv[0])->chars, &endptr, 10);
        if (errno != 0) {
            DaiObjError* err =
                DaiObjError_Newf(vm, "int() failed to convert string to int: %s", strerror(errno));
            return OBJ_VAL(err);
        }
        if (*endptr != '\0') {
            DaiObjError* err = DaiObjError_Newf(
                vm,
                "int() failed to convert string to int: invalid literal for int() with base 10");
            return OBJ_VAL(err);
        }
        return INTEGER_VAL(val);
    } else {
        DaiObjError* err = DaiObjError_Newf(
            vm, "int() expected number argument, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
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
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "int",
        .function = builtin_int,
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
        DaiObjModule_add_global(
            module, builtin_time_funcs[i].name, OBJ_VAL(&builtin_time_funcs[i]));
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

static DaiValue
builtin_math_floor(__attribute__((unused)) DaiVM* vm, __attribute__((unused)) DaiValue receiver,
                   int argc, DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "math.floor() expected 1 argument, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (IS_INTEGER(argv[0])) {
        return argv[0];
    } else if (IS_FLOAT(argv[0])) {
        double n = floor(AS_FLOAT(argv[0]));
        return INTEGER_VAL(n);
    } else {
        DaiObjError* err = DaiObjError_Newf(
            vm, "math.floor() expected number arguments, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
}

static DaiValue
builtin_math_ceil(__attribute__((unused)) DaiVM* vm, __attribute__((unused)) DaiValue receiver,
                  int argc, DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "math.ceil() expected 1 argument, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (IS_INTEGER(argv[0])) {
        return argv[0];
    } else if (IS_FLOAT(argv[0])) {
        double n = ceil(AS_FLOAT(argv[0]));
        return INTEGER_VAL(n);
    } else {
        DaiObjError* err = DaiObjError_Newf(
            vm, "math.ceil() expected number arguments, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
}

static DaiValue
builtin_math_random(__attribute__((unused)) DaiVM* vm, __attribute__((unused)) DaiValue receiver,
                    int argc, DaiValue* argv) {
    if (argc != 0) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "math.random() expected no arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    double n = (double)rand() / RAND_MAX;
    return FLOAT_VAL(n);
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
        .name     = "floor",
        .function = builtin_math_floor,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "ceil",
        .function = builtin_math_ceil,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "random",
        .function = builtin_math_random,
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
        DaiObjModule_add_global(
            module, builtin_math_funcs[i].name, OBJ_VAL(&builtin_math_funcs[i]));
    }
    return module;
}

// #endregion

// #region 内置模块 os

static DaiValue
builtin_os_getcwd(__attribute__((unused)) DaiVM* vm, __attribute__((unused)) DaiValue receiver,
                  int argc, DaiValue* argv) {
    if (argc != 0) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "os.getcwd() expected no arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        return OBJ_VAL(dai_copy_string_intern(vm, cwd, strlen(cwd)));
    }
    DaiObjError* err = DaiObjError_Newf(vm, "os.getcwd() failed: %s", strerror(errno));
    return OBJ_VAL(err);
}

static DaiValue
builtin_os_system(__attribute__((unused)) DaiVM* vm, __attribute__((unused)) DaiValue receiver,
                  int argc, DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "os.system() expected 1 argument, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (!IS_STRING(argv[0])) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "os.system() expected string arguments, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
    const char* cmd = AS_STRING(argv[0])->chars;
    int ret         = system(cmd);
    return INTEGER_VAL(ret);
}

static DaiValue
builtin_os_chdir(__attribute__((unused)) DaiVM* vm, __attribute__((unused)) DaiValue receiver,
                 int argc, DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err = DaiObjError_Newf(vm, "os.chdir() expected 1 argument, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (!IS_STRING(argv[0])) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "os.chdir() expected string arguments, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
    const char* path = AS_STRING(argv[0])->chars;
    int ret          = chdir(path);
    return INTEGER_VAL(ret);
}

static DaiObjBuiltinFunction builtin_os_funcs[] = {
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "getcwd",
        .function = builtin_os_getcwd,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "system",
        .function = builtin_os_system,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "chdir",
        .function = builtin_os_chdir,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name = NULL,
    },
};

static DaiObjModule*
builtin_os_module(DaiVM* vm) {
    DaiObjModule* module = DaiObjModule_New(vm, strdup("os"), strdup("<builtin>"));
    for (int i = 0; builtin_os_funcs[i].name != NULL; i++) {
        DaiObjModule_add_global(module, builtin_os_funcs[i].name, OBJ_VAL(&builtin_os_funcs[i]));
    }
    return module;
}

// #endregion

// #region 内置类 Path

static int path_field_index = 0;
#define STRING_NAME(name) dai_copy_string_intern(vm, name, strlen(name))

static DaiValue
builtin_path_read_text(DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 0) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "Path.read_text() expected no arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    DaiObjInstance* instance = AS_INSTANCE(receiver);
    DaiObjString* path =
        AS_STRING(instance->fields[path_field_index]);   // 这里的 path 是一个 string 对象
    char* text = dai_string_from_file(path->chars);
    if (text == NULL) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "Path.read_text() failed: %s(%s)", path->chars, strerror(errno));
        return OBJ_VAL(err);
    }
    return OBJ_VAL(dai_take_string(vm, text, strlen(text)));
}

static DaiValue
builtin_path_write_text(DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "Path.write_text() expected 1 argument, but got %d", argc);
        return OBJ_VAL(err);
    }
    DaiObjInstance* instance = AS_INSTANCE(receiver);
    DaiObjString* path =
        AS_STRING(instance->fields[path_field_index]);   // 这里的 path 是一个 string 对象
    if (!IS_STRING(argv[0])) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "Path.write_text() expected string arguments, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
    const char* text = AS_STRING(argv[0])->chars;
    FILE* fp         = fopen(path->chars, "w");
    if (fp == NULL) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "Path.write_text() failed: %s(%s)", path->chars, strerror(errno));
        return OBJ_VAL(err);
    }
    size_t len = strlen(text);
    size_t n   = fwrite(text, sizeof(char), len, fp);
    fclose(fp);
    if (n != len) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "Path.write_text() failed: %s(%s)", path->chars, strerror(errno));
        return OBJ_VAL(err);
    }
    return INTEGER_VAL(n);
}

static DaiValue
builtin_path_init(DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "Path.__init__() expected 1 argument, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (!IS_STRING(argv[0])) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "Path.__init__() expected string arguments, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
    DaiObjInstance* path           = AS_INSTANCE(receiver);
    path->fields[path_field_index] = argv[0];
    return receiver;
}

static DaiValue
built_path_exists(DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 0) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "Path.exists() expected no arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    DaiObjInstance* instance = AS_INSTANCE(receiver);
    DaiObjString* path =
        AS_STRING(instance->fields[path_field_index]);   // 这里的 path 是一个 string 对象
    return BOOL_VAL(access(path->chars, F_OK) == 0);
}

static DaiValue
built_path_unlink(DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    bool missing_ok = false;
    if (argc > 1) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "Path.unlink() expected 0 or 1 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (argc == 1) {
        if (!IS_BOOL(argv[0])) {
            DaiObjError* err = DaiObjError_Newf(
                vm, "Path.unlink() expected bool arguments, but got %s", dai_value_ts(argv[0]));
            return OBJ_VAL(err);
        }
        missing_ok = AS_BOOL(argv[0]);
    }
    DaiObjInstance* instance = AS_INSTANCE(receiver);
    DaiObjString* path =
        AS_STRING(instance->fields[path_field_index]);   // 这里的 path 是一个 string 对象
    if (unlink(path->chars) == -1) {
        if (errno == ENOENT && missing_ok) {
            return NIL_VAL;
        }
        DaiObjError* err =
            DaiObjError_Newf(vm, "Path.unlink() failed: %s(%s)", path->chars, strerror(errno));
        return OBJ_VAL(err);
    }
    return NIL_VAL;
}

static DaiValue
builtin_path_str(DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 0) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "Path.__str__() expected no arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    DaiObjInstance* instance = AS_INSTANCE(receiver);
    DaiObjString* path =
        AS_STRING(instance->fields[path_field_index]);   // 这里的 path 是一个 string 对象
    return OBJ_VAL(path);
}

static DaiValue
builtin_path_is_absolute(DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 0) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "Path.is_absolute() expected no arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    DaiObjInstance* instance = AS_INSTANCE(receiver);
    DaiObjString* path =
        AS_STRING(instance->fields[path_field_index]);   // 这里的 path 是一个 string 对象
    return BOOL_VAL(cwk_path_is_absolute(path->chars));
}

static DaiValue
builtin_path_absolute(DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 0) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "Path.absolute() expected no arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    DaiObjInstance* instance = AS_INSTANCE(receiver);
    DaiObjString* path =
        AS_STRING(instance->fields[path_field_index]);   // 这里的 path 是一个 string 对象
    if (cwk_path_is_absolute(path->chars)) {
        return receiver;
    }
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        DaiObjError* err = DaiObjError_Newf(vm, "Path.absolute() failed: %s", strerror(errno));
        return OBJ_VAL(err);
    }
    char path_buf[PATH_MAX];
    cwk_path_get_absolute(cwd, path->chars, path_buf, sizeof(path_buf));
    DaiValue abs_path            = OBJ_VAL(dai_copy_string_intern(vm, path_buf, strlen(path_buf)));
    DaiObjInstance* new_instance = DaiObjInstance_Copy(vm, instance);
    new_instance->fields[path_field_index] = abs_path;
    return OBJ_VAL(new_instance);
}

static DaiValue
builtin_path_joinpath(DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc == 0) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "Path.joinpath() expected 1 or more arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    DaiObjInstance* instance = AS_INSTANCE(receiver);
    DaiObjString* path =
        AS_STRING(instance->fields[path_field_index]);   // 这里的 path 是一个 string 对象
    char path_buf[PATH_MAX];
    char path_res[PATH_MAX];
    strcpy(path_res, path->chars);
    for (int i = 0; i < argc; i++) {
        if (!IS_STRING(argv[i])) {
            DaiObjError* err = DaiObjError_Newf(
                vm, "Path.joinpath() expected string arguments, but got %s", dai_value_ts(argv[i]));
            return OBJ_VAL(err);
        }
        const char* arg = AS_STRING(argv[i])->chars;
        cwk_path_join(path_res, arg, path_buf, sizeof(path_buf));
        strcpy(path_res, path_buf);
    }
    DaiValue joined_path = OBJ_VAL(dai_copy_string_intern(vm, path_res, strlen(path_res)));
    return DaiObjClass_call(instance->klass, vm, 1, &joined_path);
}

static DaiValue
builtin_path_parent(DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 0) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "Path.parent() expected no arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    DaiObjInstance* instance = AS_INSTANCE(receiver);
    DaiObjString* path =
        AS_STRING(instance->fields[path_field_index]);   // 这里的 path 是一个 string 对象
    size_t length;
    cwk_path_get_dirname(path->chars, &length);
    DaiValue parent_path = OBJ_VAL(
        dai_copy_string_intern(vm, path->chars, length));   // 这里的 path 是一个 string 对象
    return DaiObjClass_call(instance->klass, vm, 1, &parent_path);
}


static DaiObjBuiltinFunction builtin_path_methods[] = {
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "__init__",
        .function = builtin_path_init,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "read_text",
        .function = builtin_path_read_text,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "write_text",
        .function = builtin_path_write_text,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "exists",
        .function = built_path_exists,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "unlink",
        .function = built_path_unlink,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "string",
        .function = builtin_path_str,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "is_absolute",
        .function = builtin_path_is_absolute,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "absolute",
        .function = builtin_path_absolute,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "joinpath",
        .function = builtin_path_joinpath,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "parent",
        .function = builtin_path_parent,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name = NULL,
    },
};

static DaiObjClass*
builtin_path_class(DaiVM* vm) {
    DaiObjClass* path_class = DaiObjClass_New(vm, STRING_NAME("Path"));
    path_field_index =
        DaiObjClass_define_field(path_class, STRING_NAME("_path"), UNDEFINED_VAL, true);

    for (int i = 0; builtin_path_methods[i].name != NULL; i++) {
        DaiObjClass_define_method(path_class,
                                  STRING_NAME(builtin_path_methods[i].name),
                                  OBJ_VAL(&builtin_path_methods[i]));
    }
    return path_class;
}

// #endregion

const char* builtin_names[BUILTIN_OBJECT_MAX_COUNT]               = {};
static DaiBuiltinObject builtin_objects[BUILTIN_OBJECT_MAX_COUNT] = {};

DaiBuiltinObject*
init_builtin_objects(DaiVM* vm, int* count) {
    for (int i = 0; i < BUILTIN_OBJECT_MAX_COUNT; i++) {
        builtin_names[i]         = NULL;
        builtin_objects[i].name  = NULL;
        builtin_objects[i].value = UNDEFINED_VAL;
    }

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

    builtin_names[i]         = "os";
    builtin_objects[i].name  = "os";
    builtin_objects[i].value = OBJ_VAL(builtin_os_module(vm));
    i++;

    builtin_names[i]         = "Path";
    builtin_objects[i].name  = "Path";
    builtin_objects[i].value = OBJ_VAL(builtin_path_class(vm));
    i++;

    *count = i;
    return builtin_objects;
}
