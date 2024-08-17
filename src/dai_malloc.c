//
// Created by  on 2024/5/28.
//

#include <stdio.h>
#include <stdlib.h>

/* Allocate memory or panic */
void*
dai_malloc(size_t size) {
    void* ptr = malloc(size);
    if (!ptr) {
        fprintf(stderr, "dai_malloc: Out of memory trying to allocate %zu bytes\n", size);
        fflush(stderr);
        abort();
    }
    return ptr;
}

/* Allocate memory or panic */
void*
dai_realloc(void* ptr, size_t size) {
    void* nptr = realloc(ptr, size);
    if (!nptr) {
        fprintf(stderr, "dai_realloc: Out of memory trying to allocate %zu bytes\n", size);
        fflush(stderr);
        abort();
    }
    return nptr;
}
