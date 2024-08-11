#ifndef EA18705B_2BFD_4995_89D9_F725E8A9F455
#define EA18705B_2BFD_4995_89D9_F725E8A9F455

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define unreachable() assert(false)
#define dai_log(...) printf(__VA_ARGS__)
#define dai_error(...) fprintf(stderr, __VA_ARGS__)

// #ifndef TEST
// #define DEBUG_TRACE_EXECUTION
// // 反汇编展示变量名
// #define DISASSEMBLE_VARIABLE_NAME
// #define DEBUG_LOG_GC
// #endif

// #define DEBUG_STRESS_GC
#define UINT8_COUNT (UINT8_MAX + 1)

#endif /* EA18705B_2BFD_4995_89D9_F725E8A9F455 */
