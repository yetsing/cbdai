/*
工具函数
*/
#include "dai_utils.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#    include <bcrypt.h>
#    include <windows.h>

#else
#    include <fcntl.h>
#    include <time.h>
#    include <unistd.h>
#endif

#include "dai_windows.h"   // IWYU pragma: keep

void
pin_time_record(TimeRecord* record) {
#ifdef _WIN32
    /* 获取进程时间 */
    GetProcessTimes(GetCurrentProcess(),
                    &record->creation_time,
                    &record->exit_time,
                    &record->kernel_time,
                    &record->user_time);

    /* 获取高精度实时时钟 */
    QueryPerformanceCounter(&record->perf_counter);
#else
    /* UNIX 系统实现 */
    getrusage(RUSAGE_SELF, &record->usage);
    clock_gettime(CLOCK_MONOTONIC, &record->real_time);
#endif
}

#ifdef _WIN32
static double
filetime_to_seconds(const FILETIME* ft) {
    ULARGE_INTEGER uli;
    uli.LowPart  = ft->dwLowDateTime;
    uli.HighPart = ft->dwHighDateTime;
    return uli.QuadPart / 1e7;   // 100ns单位转换为秒
}
#endif

void
print_used_time(TimeRecord* start, TimeRecord* end) {
#ifdef _WIN32
    /* 计算CPU时间 */
    double user = filetime_to_seconds(&end->user_time) - filetime_to_seconds(&start->user_time);
    double kernel =
        filetime_to_seconds(&end->kernel_time) - filetime_to_seconds(&start->kernel_time);

    /* 计算真实时间 */
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    double real_time =
        (double)(end->perf_counter.QuadPart - start->perf_counter.QuadPart) / freq.QuadPart;

    printf("Real: %.3fs | User: %.3fs | System: %.3fs\n", real_time, user, kernel);
#else
    /* UNIX 系统计算 */
    double user = (end->usage.ru_utime.tv_sec - start->usage.ru_utime.tv_sec) +
                  (end->usage.ru_utime.tv_usec - start->usage.ru_utime.tv_usec) / 1e6;
    double system = (end->usage.ru_stime.tv_sec - start->usage.ru_stime.tv_sec) +
                    (end->usage.ru_stime.tv_usec - start->usage.ru_stime.tv_usec) / 1e6;

    double real = (end->real_time.tv_sec - start->real_time.tv_sec) +
                  (end->real_time.tv_nsec - start->real_time.tv_nsec) / 1e9;

    printf("Real: %.3fs | User: %.3fs | System: %.3fs\n", real, user, system);
#endif
}


char*
dai_string_from_file(const char* filename) {
    FILE* fp = fopen(filename, "r");
    if (fp == NULL) {
        return NULL;
    }

    // Get file size
    if (fseek(fp, 0L, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }

    long file_size = ftell(fp);
    if (file_size < 0) {
        fclose(fp);
        return NULL;
    }

    if (fseek(fp, 0L, SEEK_SET) != 0) {
        fclose(fp);
        return NULL;
    }

    // Allocate memory for file contents plus null terminator
    char* buffer = malloc((size_t)file_size + 1);
    if (buffer == NULL) {
        fclose(fp);
        return NULL;
    }

    // Read file contents
    size_t bytes_read = fread(buffer, 1, (size_t)file_size, fp);

    // Check for read errors
    if (ferror(fp)) {
        fclose(fp);
        free(buffer);
        return NULL;
    }

    fclose(fp);

    // Null-terminate the string
    buffer[bytes_read] = '\0';

    return buffer;
}

char*
dai_get_line(const char* s, int lineno) {
    const char* s1;
    for (int i = 0; i < lineno - 1; i++) {
        s1 = strchr(s, '\n');
        if (s1 == NULL) {
            return strdup("<unknown>");
        }
        s = s1 + 1;
    }
    char* line_end = strchr(s, '\n');
    if (line_end == NULL) {
        return strdup(s);
    } else {
        size_t n = line_end - s;
        return strndup(s, n);
    }
}

char*
dai_line_from_file(const char* filename, int lineno) {
    char* s = dai_string_from_file(filename);
    if (s == NULL) {
        return NULL;
    }
    char* line = dai_get_line(s, lineno);
    free(s);
    return line;
}

int
dai_generate_seed(uint8_t seed[16]) {
#ifdef _WIN32
    // Use Windows Cryptography API for secure random numbers
    if (BCryptGenRandom(NULL, seed, 16, BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
        return -1;   // Failed to generate random numbers
    }
#else
    // Use /dev/urandom on Unix-like systems
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        perror("Failed to open /dev/urandom");
        return -1;   // Failed to open /dev/urandom
    }
    if (read(fd, seed, 16) != 16) {
        perror("Failed to read random bytes");
        close(fd);
        return -1;   // Failed to read random bytes
    }
    close(fd);
#endif
    return 0;   // Success
}
