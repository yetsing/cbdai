/*
工具函数
*/
#include <stdio.h>

#include "dai_utils.h"
#include "dai_malloc.h"

void pin_time_record(TimeRecord* record) {
    getrusage(RUSAGE_SELF, &record->usage);
    clock_gettime(CLOCK_MONOTONIC, &record->tv);
}

void print_used_time(TimeRecord *start, TimeRecord *end) {
    // 计算 wall time
    long wall_seconds = end->tv.tv_sec - start->tv.tv_sec;
    long wall_nanoseconds = end->tv.tv_nsec - start->tv.tv_nsec;

    struct timeval user_tv, sys_tv;
    // 计算 user time
    timersub(&end->usage.ru_utime, &start->usage.ru_utime, &user_tv);
    // 计算 sys time
    timersub(&end->usage.ru_stime, &start->usage.ru_stime, &sys_tv);

    const char *unit;
    unsigned long wall_ts = 0;
    unsigned long sys_ts = 0;
    unsigned long user_ts = 0;
    if (wall_seconds > 0) {
        unit = "s";
        wall_ts = wall_seconds;
        sys_ts = sys_tv.tv_sec;
        user_ts = user_tv.tv_sec;
    } else if (wall_nanoseconds > 1000 * 1000) {
        unit = "ms";
        wall_ts = wall_seconds * 1000 + wall_nanoseconds / 1000 / 1000;
        sys_ts = sys_tv.tv_sec * 1000 + sys_tv.tv_usec / 1000;
        user_ts = user_tv.tv_sec * 1000 + user_tv.tv_usec / 1000;
    } else if (wall_nanoseconds > 1000) {
        unit = "µs";
        wall_ts = wall_seconds * 1000 * 1000 + wall_nanoseconds / 1000;
        sys_ts = sys_tv.tv_sec * 1000 * 1000 + sys_tv.tv_usec;
        user_ts = user_tv.tv_sec * 1000 * 1000 + user_tv.tv_usec;
    } else {
        unit = "ns";
        wall_ts = wall_seconds * 1000 * 1000 * 1000 + wall_nanoseconds;
        sys_ts = sys_tv.tv_sec * 1000 * 1000 * 1000 + sys_tv.tv_usec * 1000;
        user_ts = user_tv.tv_sec * 1000 * 1000 * 1000 + user_tv.tv_usec * 1000;
    }
    printf("CPU times: user %ld%s, system %ld%s, total %ld%s\n", user_ts, unit, sys_ts, unit, user_ts + sys_ts, unit);
    printf("Wall times: %ld%s\n", wall_ts, unit);
}


char *
dai_string_from_file(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        return NULL;
    }
    fseek(fp, 0L, SEEK_END);
    size_t length = ftell(fp);
    rewind(fp);
    char *s = dai_malloc(length + 1);
    fread(s, 1, length, fp);
    s[length] = '\0';
    fclose(fp);
    return s;
}
