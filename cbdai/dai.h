#include <stdint.h>

typedef struct Dai Dai;

Dai*
dai_new();

void
dai_free(Dai* dai);

/**
 * @brief load dai script from file. If failed, abort.
 *        It will load the file and execute it. Only can be called once.
 */
void
dai_load_file(Dai* dai, const char* filename);

/**
 * @brief get global variable int value. If not found or not int, abort.
 */
int64_t
dai_get_int(Dai* dai, const char* name);

/**
 * @brief set global variable int value. If not found, abort.
 */
void
dai_set_int(Dai* dai, const char* name, int64_t value);

/**
 * @brief get global variable float value. If not found or not float, abort.
 */
double
dai_get_float(Dai* dai, const char* name);

/**
 * @brief set global variable float value. If not found, abort.
 */
void
dai_set_float(Dai* dai, const char* name, double value);

/**
 * @brief get global variable string value. If not found or not string, abort.
 *        not change the return value.
 */
const char*
dai_get_string(Dai* dai, const char* name);

/**
 * @brief set global variable string value. It will copy the string. If not found, abort.
 */
void
dai_set_string(Dai* dai, const char* name, const char* value);

// #region Call function in dai script.
/*
 * Example:
 *   dai_func_t func = dai_get_function(dai, "my_function");
 *   DaiCall* call = dai_call_new(dai, func);
 *   dai_call_push_arg_int(call, 123);
 *   dai_call_execute(dai, call);
 *   int64_t ret_int = dai_call_return_int(call);
 *   dai_call_free(call);
 */

typedef uintptr_t dai_func_t;

/**
 * @brief get function. If not found or not function, abort.
 */
dai_func_t
dai_get_function(Dai* dai, const char* name);

// push function to call
void
daicall_push_function(Dai* dai, dai_func_t func);
// push argument to function call
void
daicall_pusharg_int(Dai* dai, int64_t value);
void
daicall_pusharg_float(Dai* dai, double value);
void
daicall_pusharg_string(Dai* dai, const char* value);
void
daicall_pusharg_function(Dai* dai, dai_func_t value);
void
daicall_pusharg_nil(Dai* dai);

/**
 * @brief execute function call. If failed, abort.
 */
void
daicall_execute(Dai* dai);

// get return value from function call
int64_t
daicall_getrv_int(Dai* dai);
double
daicall_getrv_float(Dai* dai);
const char*
daicall_getrv_string(Dai* dai);
dai_func_t
daicall_getrv_function(Dai* dai);

// #endregion

// #region Register C function to dai script
// Example:
//   void my_c_function(Dai* dai) {
//       int64_t arg1 = dai_call_pop_arg_int(dai);  // first argument
//       double arg2 = dai_call_pop_arg_float(dai);  // second argument
//       const char* arg3 = dai_call_pop_arg_string(dai); // third argument
//       dai_call_push_return_int(dai, arg1 + 1);
//    }
//    dai_register_function(dai, "my_c_function", my_c_function, 3);

typedef void (*dai_c_func_t)(Dai* dai);

/**
 * @brief register C function to dai. Must be called before dai_load_file.
 */
void
dai_register_function(Dai* dai, const char* name, dai_c_func_t func, int arity);

int64_t
daicall_poparg_int(Dai* dai);
double
daicall_poparg_float(Dai* dai);
const char*
daicall_poparg_string(Dai* dai);

void
daicall_setrv_int(Dai* dai, int64_t value);
void
daicall_setrv_float(Dai* dai, double value);
void
daicall_setrv_string(Dai* dai, const char* value);
void
daicall_setrv_nil(Dai* dai);

// #endregion
