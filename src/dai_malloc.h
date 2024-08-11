/*
封装内存操作，用于词法分析、语法分析使用
*/
#ifndef CBDAI_SRC_DAI_MALLOC_H_
#define CBDAI_SRC_DAI_MALLOC_H_

#include <stdlib.h>

/* Allocate memory or panic */
void *dai_malloc(size_t size);
/* Allocate memory or panic */
void *dai_realloc(void *ptr, size_t size);

/* Free *ptr memory and set null */
#define dai_free(ptr) do { \
  free(ptr);          \
  ptr = NULL; \
} while(0)

// 转移指针的所有权
#define dai_move(src, dst) do { \
  dst = src; \
  src = NULL; \
} while(0)

#endif //CBDAI_SRC_DAI_MALLOC_H_
