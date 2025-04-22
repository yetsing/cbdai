// windows compat
#include "dai_windows.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
char * strndup(const char * src, size_t size) {
    size_t len = strnlen(src, size);
    len = len < size ? len : size;
    char * dst = malloc(len + 1);
    if (!dst)
        return NULL;
    memcpy(dst, src, len);
    dst[len] = '\0';
    return dst;
}
#endif