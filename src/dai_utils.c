/*
工具函数
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dai_utils.h"

void
pin_time_record(TimeRecord* record) {
    getrusage(RUSAGE_SELF, &record->usage);
    clock_gettime(CLOCK_MONOTONIC, &record->tv);
}

void
print_used_time(TimeRecord* start, TimeRecord* end) {
    // 计算 wall time
    long wall_seconds     = end->tv.tv_sec - start->tv.tv_sec;
    long wall_nanoseconds = end->tv.tv_nsec - start->tv.tv_nsec;

    struct timeval user_tv, sys_tv;
    // 计算 user time
    timersub(&end->usage.ru_utime, &start->usage.ru_utime, &user_tv);
    // 计算 sys time
    timersub(&end->usage.ru_stime, &start->usage.ru_stime, &sys_tv);

    const char* unit;
    unsigned long wall_ts = 0;
    unsigned long sys_ts  = 0;
    unsigned long user_ts = 0;
    if (wall_seconds > 0) {
        unit    = "s";
        wall_ts = wall_seconds;
        sys_ts  = sys_tv.tv_sec;
        user_ts = user_tv.tv_sec;
    } else if (wall_nanoseconds > 1000 * 1000) {
        unit    = "ms";
        wall_ts = wall_seconds * 1000 + wall_nanoseconds / 1000 / 1000;
        sys_ts  = sys_tv.tv_sec * 1000 + sys_tv.tv_usec / 1000;
        user_ts = user_tv.tv_sec * 1000 + user_tv.tv_usec / 1000;
    } else if (wall_nanoseconds > 1000) {
        unit    = "µs";
        wall_ts = wall_seconds * 1000 * 1000 + wall_nanoseconds / 1000;
        sys_ts  = sys_tv.tv_sec * 1000 * 1000 + sys_tv.tv_usec;
        user_ts = user_tv.tv_sec * 1000 * 1000 + user_tv.tv_usec;
    } else {
        unit    = "ns";
        wall_ts = wall_seconds * 1000 * 1000 * 1000 + wall_nanoseconds;
        sys_ts  = sys_tv.tv_sec * 1000 * 1000 * 1000 + sys_tv.tv_usec * 1000;
        user_ts = user_tv.tv_sec * 1000 * 1000 * 1000 + user_tv.tv_usec * 1000;
    }
    printf("CPU times: user %ld%s, system %ld%s, total %ld%s\n",
           user_ts,
           unit,
           sys_ts,
           unit,
           user_ts + sys_ts,
           unit);
    printf("Wall times: %ld%s\n", wall_ts, unit);
}


char*
dai_string_from_file(const char* filename) {
    FILE* fp = fopen(filename, "r");
    if (fp == NULL) {
        return NULL;
    }
    if (fseek(fp, 0L, SEEK_END) < 0) {
        fclose(fp);
        return NULL;
    }
    size_t length = ftell(fp);
    rewind(fp);
    char* s = malloc(length + 1);
    if (s == NULL) {
        fclose(fp);
        return NULL;
    }
    if (fread(s, 1, length, fp) < length) {
        fclose(fp);
        return NULL;
    }
    s[length] = '\0';
    fclose(fp);
    return s;
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
