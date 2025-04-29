#ifndef CBDAI_DAI_STRINGBUFFER_H
#define CBDAI_DAI_STRINGBUFFER_H
#include <stdint.h>

typedef struct _DaiStringBuffer DaiStringBuffer;

DaiStringBuffer*
DaiStringBuffer_New();
void
DaiStringBuffer_write(DaiStringBuffer* sb, const char* str);
void
DaiStringBuffer_writef(DaiStringBuffer* sb, const char* fmt, ...)
    __attribute__((format(printf, 2, 3)));
void
DaiStringBuffer_writen(DaiStringBuffer* sb, const char* str, size_t n);
void
DaiStringBuffer_writec(DaiStringBuffer* sb, char c);
// 每行添加前缀
void
DaiStringBuffer_writeWithLinePrefix(DaiStringBuffer* sb, char* str, const char* prefix);
void
DaiStringBuffer_writeInt(DaiStringBuffer* sb, int n);
void
DaiStringBuffer_writeInt64(DaiStringBuffer* sb, int64_t n);
void
DaiStringBuffer_writeDouble(DaiStringBuffer* sb, double n);
void
DaiStringBuffer_writePointer(DaiStringBuffer* sb, void* ptr);
size_t
DaiStringBuffer_length(DaiStringBuffer* sb);
// 返回分配的字符串，并释放 DaiStringBuffer
char*
DaiStringBuffer_getAndFree(DaiStringBuffer* sb, size_t* length);
void
DaiStringBuffer_free(DaiStringBuffer* sb);

#endif /* CBDAI_DAI_STRINGBUFFER_H */
