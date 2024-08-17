/*
字符编解码
*/
#include <stdio.h>

#include "dai_codecs.h"

// 计算二进制中高位 1 的个数
static int
count_high_bits(unsigned char c) {
    int count = 0;
    for (int i = 7; i >= 0; i--) {
        if (c & (1 << i)) {
            count++;
        } else {
            break;
        }
    }
    return count;
}

/*
ref: https://zh.wikipedia.org/wiki/UTF-8#UTF-8%E7%9A%84%E7%B7%A8%E7%A2%BC%E6%96%B9%E5%BC%8F
码点的位数	码点起值	码点终值	字节序列	Byte 1	Byte 2	Byte 3	Byte 4	Byte 5	Byte 6
 7	       U+0000    U+007F	    1	      0xxxxxxx
11	       U+0080    U+07FF	    2	      110xxxxx	10xxxxxx
16	       U+0800    U+FFFF	    3	      1110xxxx	10xxxxxx	10xxxxxx
21	       U+10000   U+1FFFFF	4	      11110xxx	10xxxxxx	10xxxxxx	10xxxxxx
26	       U+200000	 U+3FFFFFF	5	      111110xx	10xxxxxx	10xxxxxx	10xxxxxx	10xxxxxx
31	       U+4000000 U+7FFFFFFF	6	      1111110x	10xxxxxx	10xxxxxx	10xxxxxx	10xxxxxx
10xxxxxx
*/
int
dai_utf8_decode(const char* str, dai_rune_t* rune) {
    int bitcount = count_high_bits(*str);
    *rune        = DAI_INVALID_RUNE;
    // 检查是否为有效的UTF-8编码
    for (int i = 1; i < bitcount; i++) {
        if ((str[i] & 0xC0) != 0x80) {
            return -1;
        }
    }
    switch (bitcount) {
        case 0: {
            *rune = (dai_rune_t)*str;
            return 1;
        }
        case 2: {
            *rune = (dai_rune_t)((str[0] & 0x1F) << 6) | (str[1] & 0x3F);
            if (*rune < 0x80) {
                return -1;
            }
            return 2;
        }
        case 3: {
            *rune = (dai_rune_t)((str[0] & 0xF) << 12) | ((str[1] & 0x3F) << 6) | (str[2] & 0x3F);
            if (*rune < 0x800) {
                return -1;
            }
            return 3;
        }
        case 4: {
            *rune = (dai_rune_t)((str[0] & 0x7) << 18) | ((str[1] & 0x3F) << 12) |
                    ((str[2] & 0x3F) << 6) | (str[3] & 0x3F);
            if (*rune < 0x10000) {
                return -1;
            }
            return 4;
        }
        case 5: {
            *rune = (dai_rune_t)((str[0] & 0x3) << 24) | ((str[1] & 0x3F) << 18) |
                    ((str[2] & 0x3F) << 12) | ((str[3] & 0x3F) << 6) | (str[4] & 0x3F);
            if (*rune < 0x200000) {
                return -1;
            }
            return 5;
        }
        case 6: {
            *rune = (dai_rune_t)((str[0] & 0x1) << 30) | ((str[1] & 0x3F) << 24) |
                    ((str[2] & 0x3F) << 18) | ((str[3] & 0x3F) << 12) | ((str[4] & 0x3F) << 6) |
                    (str[5] & 0x3F);
            if (*rune < 0x4000000) {
                return -1;
            }
            return 6;
        }
        default: return -1;
    }
}

size_t
dai_utf8_strlen(const char* s) {
    size_t     len  = 0;
    dai_rune_t rune = 0;
    while (*s) {
        int count_of_bytes = dai_utf8_decode(s, &rune);
        s += count_of_bytes;
        len++;
    }
    return len;
}
