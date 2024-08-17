#ifndef C7C10EF7_35B4_4607_B1BC_D808350558C2
#define C7C10EF7_35B4_4607_B1BC_D808350558C2

#include <stdbool.h>

typedef struct {
    const char* start;
    int         length;
} atstr_t;

extern const atstr_t atstr_nil;

bool
atstr_isnil(atstr_t atstr);

atstr_t
atstr_new(const char* s);

char*
atstr_copy(atstr_t atstr);

bool
atstr_eq1(atstr_t atstr1, atstr_t atstr2);

bool
atstr_eq2(atstr_t atstr1, const char* s2);

// 返回下一个分隔出来的字符串
// 如果没有分隔，返回 atstr_nil
// 传入的 atstr 会被改变
atstr_t
atstr_split_next(atstr_t* atstr);
atstr_t
atstr_splitc_next(atstr_t* atstr, char delimiter);

#define ATSTR_SPLITN_WRAPPER(atstr, n, result, ret) \
    atstr_t result[n];                              \
    ret = atstr_splitn(atstr, result, n);

// 推荐使用 ATSTR_SPLITN_WRAPPER 宏
// 返回分隔的个数，分隔出来的字符串放在 result 中
// 调用者负责保证 result 有足够的空间
// 传入的 atstr 会被改变
int
atstr_splitn(atstr_t* atstr, atstr_t* result, int n);

#define ATSTR_SPLITCN_WRAPPER(atstr, delimiter, n, result, ret) \
    atstr_t result[n];                                          \
    ret = atstr_splitcn(atstr, delimiter, result, n);

// 推荐使用 ATSTR_SPLITN2_WRAPPER 宏
// 返回分隔的个数，分隔出来的字符串放在 result 中
// 调用者负责保证 result 有足够的空间
// 传入的 atstr 会被改变
int
atstr_splitcn(atstr_t* atstr, char delimiter, atstr_t* result, int n);

#endif /* C7C10EF7_35B4_4607_B1BC_D808350558C2 */
