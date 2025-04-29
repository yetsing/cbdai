#ifndef CBDAI_DAI_COMMON_H
#define CBDAI_DAI_COMMON_H

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// code from munit.h
#if defined(__GNUC__)
#    define DAI_LIKELY(expr) (__builtin_expect((expr), 1))
#    define DAI_UNLIKELY(expr) (__builtin_expect((expr), 0))
#    define DAI_UNUSED __attribute__((__unused__))
#else
#    define DAI_LIKELY(expr) (expr)
#    define DAI_UNLIKELY(expr) (expr)
#    define DAI_UNUSED
#endif

#define unreachable() assert(false)
#define dai_log(...) printf(__VA_ARGS__)
#define dai_error(fmt, ...) fprintf(stderr, fmt " [%s:%d]", ##__VA_ARGS__, __FILE__, __LINE__)
#define dai_loggc(fmt, ...) fprintf(stdout, fmt, ##__VA_ARGS__)

// #define DEBUG_TRACE_EXECUTION
// 反汇编展示变量名
// #define DISASSEMBLE_VARIABLE_NAME
// #define DEBUG_LOG_GC
// #define DEBUG_STRESS_GC

#define UINT8_COUNT (UINT8_MAX + 1)

#endif /* CBDAI_DAI_COMMON_H */
