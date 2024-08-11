#ifndef A8085DED_B87A_42E4_8C55_CA871EC13A08
#define A8085DED_B87A_42E4_8C55_CA871EC13A08

#include <stdint.h>

// 将字符串解析为整数， base 为进制，取值为 [2, 36] 
// 字符串可以以 “+” 或 “-” 开头
// 前面也可以有 0b 0o 0x 等前缀，但是进制会以传入的 base 为准
// 如果有前缀，前缀要和 base 一致
// error 为 NULL 时表示解析成功，否则解析失败，error 指向错误信息
int64_t
dai_parseint(const char *str, int base, char **error);

#endif /* A8085DED_B87A_42E4_8C55_CA871EC13A08 */
