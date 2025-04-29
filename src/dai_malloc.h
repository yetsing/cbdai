/*
封装内存操作，用于词法分析、语法分析使用
*/
#ifndef CBDAI_DAI_MALLOC_H
#define CBDAI_DAI_MALLOC_H

#include <stdlib.h>

#define GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity) * 2)

/* Allocate memory or panic */
void*
dai_malloc(size_t size);
/* Allocate memory or panic */
void*
dai_realloc(void* ptr, size_t size);

/* Free *ptr memory and set null */
#define dai_free(ptr) \
    do {              \
        free(ptr);    \
        ptr = NULL;   \
    } while (0)

// 转移指针的所有权
#define dai_move(src, dst) \
    do {                   \
        dst = src;         \
        src = NULL;        \
    } while (0)

#endif /* CBDAI_DAI_MALLOC_H */
