// windows compat
#include "dai_windows.h"   // IWYU pragma: keep

#ifdef _WIN32
#    include <stdint.h>
#    include <stdlib.h>
#    include <string.h>

// code from musl https://www.musl-libc.org/
char*
strndup(const char* s, size_t n) {
    size_t l = strnlen(s, n);
    char* d  = malloc(l + 1);
    if (!d) return NULL;
    memcpy(d, s, l);
    d[l] = 0;
    return d;
}
#endif