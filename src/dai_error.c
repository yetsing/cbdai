#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "dai_error.h"
#include "dai_malloc.h"
#include "dai_utils.h"
#include "dai_windows.h"   // IWYU pragma: keep

static char*
get_line_at(const char* s, int lineno) {
    char* s1 = (char*)s;
    for (int i = 0; i < lineno - 1; i++) {
        s1 = strchr(s, '\n');
        if (s1 == NULL) {
            break;
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

static DaiError*
DaiError_New(const char* name, char* msg, const char* filename, int lineno, int column) {
    assert(msg != NULL);
    DaiError* err = dai_malloc(sizeof(DaiError));

    err->msg = strdup(msg);
    err->pos = (DaiFilePos){
        .filename = filename,
        .lineno   = lineno,
        .column   = column,
    };
    err->name = name;
    return err;
}

static DaiError*
DaiError_Newf(const char* name, const char* filename, int lineno, int column, const char* fmt,
              va_list arg) {
    size_t len;
    // 获取格式化后的长度
    {
        // 同一个 va_list 不能多次使用
        va_list arg1;
        va_copy(arg1, arg);
        len = vsnprintf(NULL, 0, fmt, arg1);
    }
    char* msg = dai_malloc(len + 1);
    vsnprintf(msg, len + 1, fmt, arg);

    DaiError* err     = dai_malloc(sizeof(DaiError));
    err->msg          = msg;
    err->pos.filename = filename;
    err->pos.lineno   = lineno;
    err->pos.column   = column;
    err->name         = name;
    return err;
}

void
DaiError_setFilename(DaiError* err, const char* filename) {
    err->pos.filename = filename;
}

DaiSyntaxError*
DaiSyntaxError_New(char* msg, int lineno, int column) {
    return DaiError_New("SyntaxError", msg, NULL, lineno, column);
}
DaiSyntaxError*
DaiSyntaxError_Newf(int lineno, int column, const char* fmt, ...) {
    va_list arg;
    va_start(arg, fmt);
    DaiError* err = DaiError_Newf("SyntaxError", NULL, lineno, column, fmt, arg);
    va_end(arg);
    return (DaiSyntaxError*)err;
}

DaiCompileError*
DaiCompileError_New(char* msg, const char* filename, int lineno, int column) {
    return DaiError_New("CompileError", msg, filename, lineno, column);
}

DaiCompileError*
DaiCompileError_Newf(const char* filename, int lineno, int column, const char* fmt, ...) {
    va_list arg;
    va_start(arg, fmt);
    DaiError* err = DaiError_Newf("CompileError", filename, lineno, column, fmt, arg);
    va_end(arg);
    return (DaiCompileError*)err;
}

char*
DaiError_string(DaiError* error) {
    size_t bufsize = strlen(error->msg) + 256;
    char* buf      = dai_malloc(bufsize);
    snprintf(buf,
             bufsize,
             "%s: %s in %s:%d:%d",
             error->name,
             error->msg,
             error->pos.filename,
             error->pos.lineno,
             error->pos.column);
    return buf;
}

void
DaiError_free(DaiError* err) {
    dai_free(err->msg);
    dai_free(err);
}
void
DaiError_pprint(DaiError* err, const char* code) {
    char* line = get_line_at(code, err->pos.lineno);
    printf("  File \"%s\", line %d\n    %s\n    %*s^--- here\n%s: %s\n",
           err->pos.filename,
           err->pos.lineno,
           line,
           err->pos.column - 1,
           "",
           err->name,
           err->msg);
    free(line);
}