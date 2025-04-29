/*
工具函数
*/
#ifndef CBDAI_DAI_UTILS_H
#define CBDAI_DAI_UTILS_H

#include <stddef.h>
#include <stdint.h>
#ifdef _WIN32
#    include <windows.h>
#else
#    include <sys/resource.h>
#    include <sys/time.h>
#endif

typedef struct {
#ifdef _WIN32
    FILETIME creation_time;
    FILETIME exit_time;
    FILETIME kernel_time;
    FILETIME user_time;
    LARGE_INTEGER perf_counter;
#else
    struct rusage usage;
    struct timespec real_time;
#endif
} TimeRecord;

void
pin_time_record(TimeRecord* record);
void
print_used_time(TimeRecord* start, TimeRecord* end);

// 返回 NULL 表示错误，否则返回字符串
// 调用方需要释放返回的字符串
char*
dai_string_from_file(const char* filename);

// 返回 NULL 表示错误，否则返回字符串
// 调用方需要释放返回的字符串
// lineno 从 1 开始
char*
dai_get_line(const char* s, int lineno);

// 返回 NULL 表示错误，否则返回字符串
// 调用方需要释放返回的字符串
// lineno 从 1 开始
char*
dai_line_from_file(const char* filename, int lineno);

// 生成随机数种子，返回 0 表示成功，-1 表示失败
int
dai_generate_seed(uint8_t seed[16]);

typedef struct {
    char* elements;
    size_t length;
    size_t capacity;
    size_t elsize;
} DaiRawArray;

void
DaiRawArray_init(DaiRawArray* array, size_t elsize);
void
DaiRawArray_reset(DaiRawArray* array);
void
DaiRawArray_append(DaiRawArray* array, const void* element);

typedef struct {
    const char* filename;   // 只是一个引用，不管理其内存
    // lineno column 从 1 开始计数
    int lineno;
    int column;
} DaiFilePos;

#endif /* CBDAI_DAI_UTILS_H */
