#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "dai_malloc.h"
#include "dai_stringbuffer.h"

typedef struct _DaiStringBuffer {
    size_t size;
    size_t length;
    char*  data;
} DaiStringBuffer;

DaiStringBuffer*
DaiStringBuffer_New() {
    DaiStringBuffer* sb = dai_malloc(sizeof(DaiStringBuffer));
    sb->size            = 0;
    sb->length          = 0;
    sb->data            = NULL;
    return sb;
}

void
DaiStringBuffer_write(DaiStringBuffer* sb, const char* str) {
    DaiStringBuffer_writen(sb, str, strlen(str));
}

// ref: https://stackoverflow.com/a/35479641
void
DaiStringBuffer_writef(DaiStringBuffer* sb, const char* fmt, ...) {
    va_list arg;
    size_t  len;

    va_start(arg, fmt);
    len = vsnprintf(NULL, 0, fmt, arg);
    va_end(arg);

    if (sb->length + len + 1 > sb->size) {
        size_t wantsize = sb->length + len + 1;
        size_t newsize  = sb->size + sb->size;
        if (wantsize > newsize) {
            newsize = wantsize;
        }
        sb->data = dai_realloc(sb->data, newsize);
    }
    va_start(arg, fmt);
    vsnprintf(sb->data + sb->length, len + 1, fmt, arg);
    va_end(arg);
    sb->length += len;
    sb->data[sb->length] = 0;
}

void
DaiStringBuffer_writen(DaiStringBuffer* sb, const char* str, size_t n) {
    if (sb->length + n + 1 > sb->size) {
        size_t wantsize = sb->length + n + 1;
        size_t newsize  = sb->size + sb->size;
        if (wantsize > newsize) {
            newsize = wantsize;
        }
        sb->data = dai_realloc(sb->data, newsize);
    }
    memcpy(sb->data + sb->length, str, n);
    sb->length += n;
    sb->data[sb->length] = 0;
}

void
DaiStringBuffer_writeWithLinePrefix(DaiStringBuffer* sb, char* str, const char* prefix) {
    // 如果 str 以换行符结尾，需要添加换行符
    // 比如 "hello/n" 经过 strtok 之后会变成 "hello"
    size_t length          = strlen(str);
    bool   endsWithNewLine = str[length - 1] == '\n';
    char*  ptr             = strtok(str, "\n");
    while (ptr != NULL) {
        DaiStringBuffer_write(sb, prefix);
        DaiStringBuffer_write(sb, ptr);
        ptr = strtok(NULL, "\n");
        if (ptr != NULL) {
            DaiStringBuffer_write(sb, "\n");
        }
    }
    if (endsWithNewLine) {
        DaiStringBuffer_write(sb, "\n");
    }
}

void
DaiStringBuffer_writeInt(DaiStringBuffer* sb, int n) {
    char buffer[32];
    int  len = snprintf(buffer, 32, "%d", n);
    DaiStringBuffer_writen(sb, buffer, len);
}

void
DaiStringBuffer_writeInt64(DaiStringBuffer* sb, int64_t n) {
    char buffer[32];
    int  len = snprintf(buffer, 32, "%" PRId64, n);
    DaiStringBuffer_writen(sb, buffer, len);
}

void
DaiStringBuffer_writePointer(DaiStringBuffer* sb, void* ptr) {
    char buffer[32];
    int  len = snprintf(buffer, 32, "%p", ptr);
    DaiStringBuffer_writen(sb, buffer, len);
}

size_t
DaiStringBuffer_length(DaiStringBuffer* sb) {
    return sb->length;
}

// 返回分配的字符串，并释放 DaiStringBuffer
char*
DaiStringBuffer_getAndFree(DaiStringBuffer* sb, size_t* length) {
    if (length != NULL) {
        *length = sb->length;
    }
    char* s  = sb->data;
    sb->data = NULL;
    dai_free(sb);
    return s;
}
