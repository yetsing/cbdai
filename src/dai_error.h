#ifndef A3530368_FF1B_4382_89C3_A117897BF660
#define A3530368_FF1B_4382_89C3_A117897BF660

typedef struct _DaiError DaiError;

void
DaiError_setFilename(DaiError* err, const char* filename);
// 返回错误的完整字符串
// 调用方需要释放返回的字符串
// 格式：Error: msg in filename:lineno:column
char*
DaiError_string(DaiError* err);
void
DaiError_free(DaiError* err);
// 输出更友好的错误信息
void
DaiError_pprint(DaiError* err, const char* code);

#define DaiSyntaxError DaiError
#define DaiSyntaxError_setFilename DaiError_setFilename
#define DaiSyntaxError_string DaiError_string
#define DaiSyntaxError_free DaiError_free
#define DaiSyntaxError_pprint DaiError_pprint

DaiSyntaxError*
DaiSyntaxError_New(char* msg, int lineno, int column);
DaiSyntaxError*
DaiSyntaxError_Newf(int lineno, int column, const char* fmt, ...)
    __attribute__((format(printf, 3, 4)));

#define DaiCompileError DaiError
#define DaiCompileError_string DaiError_string
#define DaiCompileError_free DaiError_free
#define DaiCompileError_pprint DaiError_pprint

DaiCompileError*
DaiCompileError_New(char* msg, const char* filename, int lineno, int column);
DaiCompileError*
DaiCompileError_Newf(const char* filename, int lineno, int column, const char* fmt, ...)
    __attribute__((format(printf, 4, 5)));

#define DaiRuntimeError DaiError
#define DaiRuntimeError_string DaiError_string
#define DaiRuntimeError_free DaiError_free
#define DaiRuntimeError_pprint DaiError_pprint

DaiRuntimeError*
DaiRuntimeError_New(char* msg, const char* filename, int lineno, int column);
DaiRuntimeError*
DaiRuntimeError_Newf(const char* filename, int lineno, int column, const char* fmt, ...)
    __attribute__((format(printf, 4, 5)));
#endif /* A3530368_FF1B_4382_89C3_A117897BF660 */
