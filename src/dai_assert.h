#ifndef B3714F2B_3668_44DA_94B6_7C4516C32529
#define B3714F2B_3668_44DA_94B6_7C4516C32529

#include <assert.h>

#define daiassert(cond, ...)              \
    do {                                  \
        if (!(cond)) {                    \
            fprintf(stderr, __VA_ARGS__); \
            assert(cond);                 \
        }                                 \
    } while (0)

#endif /* B3714F2B_3668_44DA_94B6_7C4516C32529 */
