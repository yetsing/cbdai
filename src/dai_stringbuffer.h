#ifndef CBDAI_DAI_STRINGBUFFER_H
#define CBDAI_DAI_STRINGBUFFER_H

#include <stddef.h>
#include <stdint.h>

// DaiStringBuffer 是一个动态字符串缓冲区，
// 用于存储字符串数据。它会自动扩展以容纳更多数据。
// DaiStringBuffer 结构体的实现细节是隐藏的，
// 以便于用户使用时不需要关心其内部实现。
typedef struct _DaiStringBuffer DaiStringBuffer;

// 创建一个新的 DaiStringBuffer 实例。
DaiStringBuffer*
DaiStringBuffer_New();
// 释放 DaiStringBuffer 实例。
void
DaiStringBuffer_free(DaiStringBuffer* sb);
// 返回当前缓冲区的字符串长度。
size_t
DaiStringBuffer_length(DaiStringBuffer* sb);
// 返回分配的字符串，并释放 DaiStringBuffer
char*
DaiStringBuffer_getAndFree(DaiStringBuffer* sb, size_t* length);
// 写入字符串到缓冲区。
void
DaiStringBuffer_write(DaiStringBuffer* sb, const char* str);
// 写入格式化字符串到缓冲区。
void
DaiStringBuffer_writef(DaiStringBuffer* sb, const char* fmt, ...)
    __attribute__((format(printf, 2, 3)));
// 写入字符串到缓冲区，指定长度。
void
DaiStringBuffer_writen(DaiStringBuffer* sb, const char* str, size_t n);
// 写入字符到缓冲区。
void
DaiStringBuffer_writec(DaiStringBuffer* sb, char c);
// 给字符串每行添加前缀后，写入到缓冲区。
void
DaiStringBuffer_writeWithLinePrefix(DaiStringBuffer* sb, char* str, const char* prefix);
// 写入整数到缓冲区。
void
DaiStringBuffer_writeInt(DaiStringBuffer* sb, int n);
// 写入 int64_t 到缓冲区。
void
DaiStringBuffer_writeInt64(DaiStringBuffer* sb, int64_t n);
// 写入 double 到缓冲区。
void
DaiStringBuffer_writeDouble(DaiStringBuffer* sb, double n);
// 写入指针到缓冲区。
void
DaiStringBuffer_writePointer(DaiStringBuffer* sb, void* ptr);
// 缓冲区扩容到指定大小。
void
DaiStringBuffer_grow(DaiStringBuffer* sb, size_t size);
// 返回缓冲区的最后一个字符。
char
DaiStringBuffer_last(DaiStringBuffer* sb);

#endif /* CBDAI_DAI_STRINGBUFFER_H */
