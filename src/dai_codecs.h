/*
字符编解码
*/
#ifndef CBDAI_DAI_CODECS_H
#define CBDAI_DAI_CODECS_H

#include <stddef.h>
#include <stdint.h>

#define DAI_INVALID_RUNE 0x80000000

typedef int32_t dai_rune_t;

// UTF-8 解码
// 返回大于 0 的整数表示解码的字节数
// 返回 -1 表示解码错误
int
dai_utf8_decode(const char* str, dai_rune_t* rune);

// UTF-8 字符串长度
size_t
dai_utf8_strlen(const char* s);
size_t
dai_utf8_strlen2(const char* s, size_t length);

#endif /* CBDAI_DAI_CODECS_H */
