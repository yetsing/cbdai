#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dai_assert.h"
#include "dai_parseint.h"

// https://github.com/bminor/musl/blob/master/src/internal/intscan.c#L6
/* Lookup table for digit values. -1==255>=36 -> invalid */
static const unsigned char table[] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  -1, -1, -1, -1, -1, -1, -1, 10, 11, 12, 13, 14, 15,
    16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, -1, -1, -1, -1,
    -1, -1, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
    32, 33, 34, 35, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

// ** 规则变动，下面的检查函数已经没用 **
// 代码思路参照
// https://cs.opensource.google/go/go/+/refs/tags/go1.22.3:src/strconv/atoi.go
// 检查数字字符串中的下划线是否满足要求
// 规则如下：
//    下划线不能出现在开头或结尾，例如 "_1" "1_" 不行
//    下划线前后必须是数字，例如 "1__2" 不行
//    下划线前面可以是进制的表示，例如 "0x_123" 是可以的
__attribute__((unused)) static bool
dai_underscore_ok(const char* str, const size_t len) {
    // saw 追踪我们看到的最后一个字符
    // ^ 表示开始
    // 0 表示数字或者进制前缀
    // _ 表示下划线
    // ! 表示除了上面之外的字符
    char saw = '^';
    int i    = 0;

    // 去掉前面的符号
    if (len >= 1 && (str[0] == '+' || str[0] == '-')) {
        i++;
    }

    // 去掉前面的进制前缀
    if (len >= 2 && (str[0] == '0' && (str[1] == 'b' || str[1] == 'B' || str[1] == 'o' ||
                                       str[1] == 'O' || str[1] == 'x' || str[1] == 'X'))) {
        i += 2;
        saw = '0';
    }

    const unsigned char* val = table + 1;   // 这行按照 musl 的实现来，不清楚为什么要 +1
    for (; i < len; i++) {
        // 数字字符
        if (val[(int)str[i]] < 36) {
            saw = '0';
            continue;
        }
        // 下划线前面必须是数字
        if (str[i] == '_') {
            if (saw != '0') {
                return false;
            }
            saw = '_';
            continue;
        }
        // 下划线后面必须是数字
        if (saw == '_') {
            return false;
        }
        // 非数字、非下划线
        saw = '!';
    }
    return saw != '_';
}

// 代码思路参照
// https://cs.opensource.google/go/go/+/refs/tags/go1.22.3:src/strconv/atoi.go
// 解析 uint64 数字字符串，不支持 + - 符号前缀
static uint64_t
dai_parseuint(const char* str, const int base, char** error) {
    daiassert(str != NULL, "str is NULL");
    *error = NULL;
    // 保存一份，用于后面检查
    // const char* str0 = str;

    // 检查 base
    if (base < 2 || base > 36) {
        *error = "invalid base";
        return 0;
    }

    size_t slen = strlen(str);
    // 检查特殊长度的情况
    if (slen == 0) {
        *error = "empty string";
        return 0;
    }
    const unsigned char* val = table + 1;   // 这行按照 musl 的实现来，不清楚为什么要 +1
    if (slen == 1) {
        unsigned char d = val[(int)str[0]];
        if (d >= base) {
            *error = "invalid character in number";
            return 0;
        }
        return d;
    }

    // 检查进制前缀
    if (str[0] == '0') {
        int base0 = 0;
        switch (base) {
            case 2: {
                if (slen >= 3 && (str[1] == 'b' || str[1] == 'B')) {
                    base0 = 2;
                }
                str += 2;
                break;
            }
            case 8: {
                if (slen >= 3 && (str[1] == 'o' || str[1] == 'O')) {
                    base0 = 8;
                }
                str += 2;
                break;
            }
            case 16: {
                if (slen >= 3 && (str[1] == 'x' || str[1] == 'X')) {
                    base0 = 16;
                }
                str += 2;
                break;
            }
        }
        if (base0 == 0 || base0 != base) {
            *error = "invalid base prefix";
            return 0;
        }
    }

    // 计算边界值
    // Cutoff is the smallest number such that cutoff*base > maxUint64.
    // Use compile-time constants for common cases.
    uint64_t cutoff = 0;
    switch (base) {
        case 10: cutoff = UINT64_MAX / 10 + 1; break;
        case 16: cutoff = UINT64_MAX / 16 + 1; break;
        default: cutoff = UINT64_MAX / base + 1; break;
    }

    // 解析字符串
    // bool underscores = false;   // 是否包含下划线 _
    uint64_t n = 0;
    for (; *str != 0; str++) {
        char c = *str;
        if (c == '_') {
            // underscores = true;
            continue;
        }
        unsigned char d = val[(int)c];
        // 是否有效字符
        if (d >= base) {
            *error = "invalid character in number";
            return 0;
        }
        // 检查 n*base 是否溢出
        if (n >= cutoff) {
            *error = "integer overflow";
            return 0;
        }
        n *= base;
        // 检查 n+d 是否溢出
        uint64_t n1 = n + d;
        if (n1 < n) {
            *error = "integer overflow";
            return 0;
        }
        n = n1;
    }

    // 词法分析已经检查完整数格式
    // if (underscores && !dai_underscore_ok(str0, slen)) {
    //     *error = "invalid underscores usage in number";
    //     return 0;
    // }

    return n;
}

// 代码思路参照
// https://cs.opensource.google/go/go/+/refs/tags/go1.22.3:src/strconv/atoi.go
int64_t
dai_parseint(const char* str, int base, char** error) {
    daiassert(str != NULL, "str is NULL");
    *error = NULL;

    // 检查 base
    if (base < 2 || base > 36) {
        *error = "invalid base";
        return 0;
    }
    // 检查符号
    bool neg = 0;
    if (*str == '+') {
        str++;
    } else if (*str == '-') {
        str++;
        neg = 1;
    }

    // 先按无符号整数解析
    uint64_t un = dai_parseuint(str, base, error);
    if (*error != NULL) {
        return 0;
    }
    // 检查溢出
    if (!neg && un > (uint64_t)INT64_MAX) {
        *error = "integer overflow";
        return 0;
    }
    // 负数比正数多一
    if (neg && un > (uint64_t)INT64_MAX + 1) {
        *error = "integer overflow";
        return 0;
    }
    // 转为 int64_t 并处理符号
    int64_t n = (int64_t)un;
    if (neg == 1) {
        // 一个特殊情况的说明，对于 9223372036854775808 (INT64_MAX + 1) 这个数字
        // 如果 un = 9223372036854775808 ，转成 int64_t 会变成负数
        // -9223372036854775808 再取个负号，还是 -9223372036854775808
        n = -n;
    }
    return n;
}