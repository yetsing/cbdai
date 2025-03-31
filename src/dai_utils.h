/*
工具函数
*/
#ifndef AECB9C05_1769_47C9_99B4_8EB1E4235629
#define AECB9C05_1769_47C9_99B4_8EB1E4235629

#include <stdint.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <time.h>

typedef struct {
    struct rusage usage;
    struct timespec tv;
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

#endif /* AECB9C05_1769_47C9_99B4_8EB1E4235629 */
