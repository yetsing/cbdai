#include <ctype.h>
#include <string.h>

#include "./atstr.h"

const atstr_t atstr_nil = { .start = NULL, .length = 0 };

bool
atstr_isnil(atstr_t atstr) {
    return atstr.start == NULL && atstr.length == 0;
}

atstr_t
atstr_new(const char *s) {
    int length = strlen(s);
    return (atstr_t) {
        .start = s,
        .length = length
    };
}

char *
atstr_copy(atstr_t atstr) {
    return strndup(atstr.start, atstr.length);
}

bool
atstr_eq1(atstr_t atstr1, atstr_t atstr2) {
    return atstr1.length == atstr2.length && memcmp(atstr1.start, atstr2.start, atstr1.length) == 0;
}

bool
atstr_eq2(atstr_t atstr1, const char *s2) {
    int length = strlen(s2);
    return atstr1.length == length && memcmp(atstr1.start, s2, atstr1.length) == 0;
}

atstr_t
atstr_split_next(atstr_t *atstr) {
    // skip start whitespace
    const char *s = atstr->start;
    while (s < atstr->start + atstr->length && isspace(*s)) {
        s++;
    }
    atstr->length -= s - atstr->start;
    atstr->start = s;
    for (; s < atstr->start + atstr->length; s++) {
        if (isspace(*s)) {
            break;
        }
    }
    if (s != atstr->start) {
        int length = s - atstr->start;
        atstr_t res = (atstr_t) {
            .start = atstr->start,
            .length = length,
        };
        atstr->start += length + 1;
        atstr->length -= length + 1;
        return res;
    } else {
        return atstr_nil;
    }
}

atstr_t
atstr_splitc_next(atstr_t *atstr, char delimiter) {
    // skip start whitespace
    const char *s = atstr->start;
    while (s < atstr->start + atstr->length && isspace(*s)) {
        s++;
    }
    atstr->length -= s - atstr->start;
    atstr->start = s;
    for (; s < atstr->start + atstr->length; s++) {
        if (*s == delimiter) {
            break;
        }
    }
    if (s != atstr->start) {
        int length = s - atstr->start;
        atstr_t res = (atstr_t) {
            .start = atstr->start,
            .length = length,
        };
        atstr->start += length + 1;
        atstr->length -= length + 1;
        return res;
    } else {
        return atstr_nil;
    }
}

int
atstr_splitn(atstr_t *atstr, atstr_t *result, int n) {
    int count = 0;
    while (count < n) {
        atstr_t ret = atstr_split_next(atstr);
        if (atstr_isnil(ret)) {
            break;
        }
        result[count] = ret;
        count++;
    }
    return count;
}


int 
atstr_splitcn(atstr_t *atstr, char delimiter, atstr_t *result, int n) {
    int count = 0;
    while (count < n) {
        atstr_t ret = atstr_splitc_next(atstr, delimiter);
        if (atstr_isnil(ret)) {
            break;
        }
        result[count] = ret;
        count++;
    }
    return count;
}