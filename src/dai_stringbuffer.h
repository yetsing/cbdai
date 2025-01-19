#ifndef C759BE44_71CA_4041_B259_9DC171208F59
#define C759BE44_71CA_4041_B259_9DC171208F59
#include <stdlib.h>

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

#endif /* C759BE44_71CA_4041_B259_9DC171208F59 */
