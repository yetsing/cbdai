#ifndef CBDAI_DAI_ASSERT_H
#define CBDAI_DAI_ASSERT_H

#include <assert.h>

#define daiassert(cond, ...)              \
    do {                                  \
        if (!(cond)) {                    \
            fprintf(stderr, __VA_ARGS__); \
            assert(cond);                 \
        }                                 \
    } while (0)

#endif /* CBDAI_DAI_ASSERT_H */
