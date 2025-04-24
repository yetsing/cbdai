#include "dai_objects/dai_object_string.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "utf8.h"

#include "dai_memory.h"
#include "dai_objects/dai_object_array.h"
#include "dai_objects/dai_object_error.h"
#include "dai_stringbuffer.h"
#include "dai_value.h"
#include "dai_vm.h"

static int
utf8_one_char_length(const char* s) {
    // code copy from https://github.com/sheredom/utf8.h
    if (0xf0 == (0xf8 & *s)) {
        /* 4-byte utf8 code point (began with 0b11110xxx) */
        return 4;
    } else if (0xe0 == (0xf0 & *s)) {
        /* 3-byte utf8 code point (began with 0b1110xxxx) */
        return 3;
    } else if (0xc0 == (0xe0 & *s)) {
        /* 2-byte utf8 code point (began with 0b110xxxxx) */
        return 2;
    } else { /* if (0x00 == (0x80 & *s)) { */
        /* 1-byte ascii (began with 0b0xxxxxxx) */
        return 1;
    }
}

/**
 * @brief 返回字符串的第n个字符的指针
 *
 * @note 该函数不检查n是否越界, 请确保n不会越界
 *
 * @param str 字符串
 * @param n 第n个
 *
 * @return char*
 */
char*
utf8offset(const char* str, int n) {
    while (n > 0) {
        str += utf8_one_char_length(str);
        n--;
    }
    return (char*)str;
}

static DaiValue
DaiObjString_length(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc,
                    DaiValue* argv) {
    if (argc != 0) {
        DaiObjError* err = DaiObjError_Newf(vm, "length() expected no arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    return INTEGER_VAL(AS_STRING(receiver)->utf8_length);
}

static DaiValue
DaiObjString_format(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc,
                    DaiValue* argv) {
    DaiStringBuffer* sb  = DaiStringBuffer_New();
    DaiObjString* string = AS_STRING(receiver);
    const char* format   = string->chars;
    int i                = 0;
    int j                = 0;
    while (format[i] != '\0') {
        if (format[i] == '{' && format[i + 1] == '}') {
            if (j >= argc) {
                DaiStringBuffer_free(sb);
                DaiObjError* err = DaiObjError_Newf(vm, "format() not enough arguments");
                return OBJ_VAL(err);
            }
            DaiValue value = argv[j];
            char* s        = dai_value_string(value);
            DaiStringBuffer_write(sb, s);
            free(s);
            i += 2;
            j++;
        } else {
            int step = utf8_one_char_length(format + i);
            DaiStringBuffer_writen(sb, format + i, step);
            i += step;
        }
    }
    if (j != argc) {
        DaiStringBuffer_free(sb);
        DaiObjError* err = DaiObjError_Newf(vm, "format() too many arguments");
        return OBJ_VAL(err);
    }
    size_t length = 0;
    char* res     = DaiStringBuffer_getAndFree(sb, &length);
    return OBJ_VAL(dai_take_string(vm, res, length));
}

static DaiValue
DaiObjString_sub(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc == 0 || argc > 2) {
        DaiObjError* err = DaiObjError_Newf(vm, "sub() expected 1-2 arguments, but got 0");
        return OBJ_VAL(err);
    }
    if (!IS_INTEGER(argv[0])) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "sub() expected int arguments, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
    if (argc == 2 && !IS_INTEGER(argv[1])) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "sub() expected int arguments, but got %s", dai_value_ts(argv[1]));
        return OBJ_VAL(err);
    }
    DaiObjString* string = AS_STRING(receiver);
    int start            = AS_INTEGER(argv[0]);
    int end              = argc == 1 ? string->utf8_length : AS_INTEGER(argv[1]);
    if (start < 0) {
        start += string->utf8_length;
        if (start < 0) {
            start = 0;
        }
    }
    if (end < 0) {
        end += string->utf8_length;
    } else if (end > string->utf8_length) {
        end = string->utf8_length;
    }
    if (start >= end) {
        return OBJ_VAL(dai_copy_string(vm, "", 0));
    }
    char* s = utf8offset(string->chars, start);
    char* e = utf8offset(s, end - start);
    return OBJ_VAL(dai_copy_string(vm, s, e - s));
}

static DaiValue
DaiObjString_find(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err = DaiObjError_Newf(vm, "find() expected 1 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (!IS_STRING(argv[0])) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "find() expected string arguments, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
    DaiObjString* string = AS_STRING(receiver);
    DaiObjString* sub    = AS_STRING(argv[0]);
    if (string->length < sub->length) {
        return INTEGER_VAL(-1);
    }
    char* s = strstr(string->chars, sub->chars);
    if (s == NULL) {
        return INTEGER_VAL(-1);
    }
    return INTEGER_VAL(utf8nlen(string->chars, s - string->chars));
}

static char*
DaiObjString_replacen(DaiObjString* s, DaiObjString* old, DaiObjString* new, int max_replacements) {
    DaiStringBuffer* sb = DaiStringBuffer_New();
    const char* p       = s->chars;
    const char* end     = s->chars + s->length;
    int old_len         = old->length;
    int new_len         = new->length;
    while (p < end) {
        if (max_replacements == 0) {
            DaiStringBuffer_writen(sb, p, end - p);
            break;
        }
        const char* q = strstr(p, old->chars);
        if (q == NULL) {
            DaiStringBuffer_writen(sb, p, end - p);
            break;
        }
        DaiStringBuffer_writen(sb, p, q - p);
        DaiStringBuffer_writen(sb, new->chars, new_len);
        p = q + old_len;
        max_replacements--;
    }
    size_t length = 0;
    char* res     = DaiStringBuffer_getAndFree(sb, &length);
    return res;
}

static DaiValue
DaiObjString_replace(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc,
                     DaiValue* argv) {
    if ((argc != 2) && (argc != 3)) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "replace() expected 2-3 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (!IS_STRING(argv[0])) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "replace() expected string arguments, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
    if (!IS_STRING(argv[1])) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "replace() expected string arguments, but got %s", dai_value_ts(argv[1]));
        return OBJ_VAL(err);
    }
    if (argc == 3 && !IS_INTEGER(argv[2])) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "replace() expected int arguments, but got %s", dai_value_ts(argv[2]));
        return OBJ_VAL(err);
    }
    DaiObjString* string = AS_STRING(receiver);
    DaiObjString* old    = AS_STRING(argv[0]);
    if (old->length == 0) {
        DaiObjError* err = DaiObjError_Newf(vm, "replace() empty old string");
        return OBJ_VAL(err);
    }
    DaiObjString* new = AS_STRING(argv[1]);
    int count         = argc == 3 ? AS_INTEGER(argv[2]) : INT_MAX;
    char* res         = DaiObjString_replacen(string, old, new, count);
    return OBJ_VAL(dai_take_string(vm, res, strlen(res)));
}

static DaiValue
DaiObjString_splitn(DaiVM* vm, DaiObjString* s, DaiObjString* sep, int max_splits) {
    DaiObjArray* array = DaiObjArray_New(vm, NULL, 0);
    const char* p      = s->chars;
    const char* end    = s->chars + s->length;
    int sep_len        = sep->length;
    while (p < end) {
        if (max_splits <= 0) {
            DaiObjArray_append1(vm, array, 1, &OBJ_VAL(dai_copy_string(vm, p, end - p)));
            break;
        }
        const char* q = strstr(p, sep->chars);
        if (q == NULL) {
            DaiObjArray_append1(vm, array, 1, &OBJ_VAL(dai_copy_string(vm, p, end - p)));
            break;
        }
        DaiObjString* sub = dai_copy_string(vm, p, q - p);
        DaiObjArray_append1(vm, array, 1, &OBJ_VAL(sub));
        p = q + sep_len;
        max_splits--;
    }
    return OBJ_VAL(array);
}

static DaiValue
DaiObjString_split_whitespace(DaiVM* vm, DaiObjString* s) {
    DaiObjArray* array = DaiObjArray_New(vm, NULL, 0);
    const char* p      = s->chars;
    const char* end    = s->chars + s->length;
    while (p < end) {
        while (p < end && isspace(*p)) {
            p++;
        }
        const char* q = p;
        while (q < end && !isspace(*q)) {
            q++;
        }
        if (p < q) {
            DaiObjString* sub = dai_copy_string(vm, p, q - p);
            DaiObjArray_append1(vm, array, 1, &OBJ_VAL(sub));
        }
        p = q;
    }
    return OBJ_VAL(array);
}

static DaiValue
DaiObjString_split(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc == 0) {
        return DaiObjString_split_whitespace(vm, AS_STRING(receiver));
    }
    if ((argc != 1) && (argc != 2)) {
        DaiObjError* err = DaiObjError_Newf(vm, "split() expected 0-2 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (!IS_STRING(argv[0])) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "split() expected string arguments, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
    if (argc == 2 && !IS_INTEGER(argv[1])) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "split() expected int arguments, but got %s", dai_value_ts(argv[1]));
        return OBJ_VAL(err);
    }
    DaiObjString* string = AS_STRING(receiver);
    DaiObjString* sep    = AS_STRING(argv[0]);
    if (sep->length == 0) {
        DaiObjError* err = DaiObjError_Newf(vm, "split() empty separator");
        return OBJ_VAL(err);
    }
    // split 的 count 参数表示结果数组的最大长度，对应分割次数需要减一
    int count = argc == 2 ? AS_INTEGER(argv[1]) - 1 : INT_MAX;
    return DaiObjString_splitn(vm, string, sep, count);
}

static DaiValue
DaiObjString_join(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err = DaiObjError_Newf(vm, "join() expected 1 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (!IS_ARRAY(argv[0])) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "join() expected array arguments, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
    DaiObjString* sep  = AS_STRING(receiver);
    DaiObjArray* array = AS_ARRAY(argv[0]);
    if (array->length == 0) {
        return OBJ_VAL(dai_copy_string(vm, "", 0));
    }
    DaiStringBuffer* sb = DaiStringBuffer_New();
    for (int i = 0; i < array->length; i++) {
        if (!IS_STRING(array->elements[i])) {
            DaiStringBuffer_free(sb);
            DaiObjError* err =
                DaiObjError_Newf(vm,
                                 "join() expected array of string arguments, but got %s at %d",
                                 dai_value_ts(array->elements[i]),
                                 i);
            return OBJ_VAL(err);
        }
        DaiStringBuffer_write(sb, AS_STRING(array->elements[i])->chars);
        if (i != array->length - 1) {
            DaiStringBuffer_write(sb, sep->chars);
        }
    }
    size_t length = 0;
    char* res     = DaiStringBuffer_getAndFree(sb, &length);
    return OBJ_VAL(dai_take_string(vm, res, length));
}

static DaiValue
DaiObjString_has(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err = DaiObjError_Newf(vm, "has() expected 1 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (!IS_STRING(argv[0])) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "has() expected string arguments, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
    DaiObjString* string = AS_STRING(receiver);
    DaiObjString* sub    = AS_STRING(argv[0]);
    char* s              = strstr(string->chars, sub->chars);
    return BOOL_VAL(s != NULL);
}

static DaiValue
DaiObjString_strip(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 0) {
        DaiObjError* err = DaiObjError_Newf(vm, "strip() expected no arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    DaiObjString* string = AS_STRING(receiver);
    const char* p        = string->chars;
    const char* end      = string->chars + string->length;
    while (p < end && isspace(*p)) {
        p++;
    }
    while (end > p && isspace(*(end - 1))) {
        end--;
    }
    if (p == string->chars && end == string->chars + string->length) {
        return receiver;
    }
    return OBJ_VAL(dai_copy_string(vm, p, end - p));
}

static DaiValue
DaiObjString_startswith(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc,
                        DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "startswith() expected 1 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (!IS_STRING(argv[0])) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "startswith() expected string arguments, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
    DaiObjString* string = AS_STRING(receiver);
    DaiObjString* sub    = AS_STRING(argv[0]);
    return BOOL_VAL(strncmp(string->chars, sub->chars, sub->length) == 0);
}

static DaiValue
DaiObjString_endswith(__attribute__((unused)) DaiVM* vm, DaiValue receiver, int argc,
                      DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "endswith() expected 1 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (!IS_STRING(argv[0])) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "endswith() expected string arguments, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
    DaiObjString* string = AS_STRING(receiver);
    DaiObjString* sub    = AS_STRING(argv[0]);
    return BOOL_VAL(
        strncmp(string->chars + string->length - sub->length, sub->chars, sub->length) == 0);
}

enum DaiObjStringFunctionNo {
    DaiObjStringFunctionNo_length = 0,
    DaiObjStringFunctionNo_format,
    DaiObjStringFunctionNo_sub,
    DaiObjStringFunctionNo_find,
    DaiObjStringFunctionNo_replace,
    DaiObjStringFunctionNo_split,
    DaiObjStringFunctionNo_join,
    DaiObjStringFunctionNo_has,
    DaiObjStringFunctionNo_strip,
    DaiObjStringFunctionNo_startswith,
    DaiObjStringFunctionNo_endswith,
};

static DaiObjBuiltinFunction DaiObjStringBuiltins[] = {
    [DaiObjStringFunctionNo_length] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "length",
            .function = &DaiObjString_length,
        },
    [DaiObjStringFunctionNo_format] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "format",
            .function = &DaiObjString_format,
        },
    [DaiObjStringFunctionNo_sub] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "sub",
            .function = &DaiObjString_sub,
        },
    [DaiObjStringFunctionNo_find] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "find",
            .function = &DaiObjString_find,
        },
    [DaiObjStringFunctionNo_replace] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "replace",
            .function = &DaiObjString_replace,
        },
    [DaiObjStringFunctionNo_split] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "split",
            .function = &DaiObjString_split,
        },
    [DaiObjStringFunctionNo_join] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "join",
            .function = &DaiObjString_join,
        },
    [DaiObjStringFunctionNo_has] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "has",
            .function = &DaiObjString_has,
        },
    [DaiObjStringFunctionNo_strip] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "strip",
            .function = &DaiObjString_strip,
        },
    [DaiObjStringFunctionNo_startswith] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "startswith",
            .function = &DaiObjString_startswith,
        },
    [DaiObjStringFunctionNo_endswith] =
        {
            {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
            .name     = "endswith",
            .function = &DaiObjString_endswith,
        },
};

static DaiValue
DaiObjString_get_property(DaiVM* vm, DaiValue receiver, DaiObjString* name) {
    const char* cname = name->chars;
    if (strcmp(cname, "length") == 0) {
        return OBJ_VAL(&DaiObjStringBuiltins[DaiObjStringFunctionNo_length]);
    }
    if (strcmp(cname, "format") == 0) {
        return OBJ_VAL(&DaiObjStringBuiltins[DaiObjStringFunctionNo_format]);
    }
    if (strcmp(cname, "sub") == 0) {
        return OBJ_VAL(&DaiObjStringBuiltins[DaiObjStringFunctionNo_sub]);
    }
    if (strcmp(cname, "find") == 0) {
        return OBJ_VAL(&DaiObjStringBuiltins[DaiObjStringFunctionNo_find]);
    }
    if (strcmp(cname, "replace") == 0) {
        return OBJ_VAL(&DaiObjStringBuiltins[DaiObjStringFunctionNo_replace]);
    }
    if (strcmp(cname, "split") == 0) {
        return OBJ_VAL(&DaiObjStringBuiltins[DaiObjStringFunctionNo_split]);
    }
    if (strcmp(cname, "join") == 0) {
        return OBJ_VAL(&DaiObjStringBuiltins[DaiObjStringFunctionNo_join]);
    }
    if (strcmp(cname, "has") == 0) {
        return OBJ_VAL(&DaiObjStringBuiltins[DaiObjStringFunctionNo_has]);
    }
    if (strcmp(cname, "strip") == 0) {
        return OBJ_VAL(&DaiObjStringBuiltins[DaiObjStringFunctionNo_strip]);
    }
    if (strcmp(cname, "startswith") == 0) {
        return OBJ_VAL(&DaiObjStringBuiltins[DaiObjStringFunctionNo_startswith]);
    }
    if (strcmp(cname, "endswith") == 0) {
        return OBJ_VAL(&DaiObjStringBuiltins[DaiObjStringFunctionNo_endswith]);
    }
    DaiObjError* err = DaiObjError_Newf(vm, "'string' object has not property '%s'", name->chars);
    return OBJ_VAL(err);
}

static char*
DaiObjString_String(DaiValue value, __attribute__((unused)) DaiPtrArray* visited) {
    return strdup(AS_STRING(value)->chars);
}

static DaiValue
DaiObjString_subscript_get(DaiVM* vm, DaiValue receiver, DaiValue index) {
    if (!IS_INTEGER(index)) {
        DaiObjError* err = DaiObjError_Newf(vm, "index must be integer");
        return OBJ_VAL(err);
    }
    DaiObjString* string = AS_STRING(receiver);
    int i                = AS_INTEGER(index);
    if (i < 0) {
        i += string->utf8_length;
    }
    if (i < 0 || i >= string->utf8_length) {
        DaiObjError* err = DaiObjError_Newf(vm, "index out of range");
        return OBJ_VAL(err);
    }
    char* s = utf8offset(string->chars, i);
    return OBJ_VAL(dai_copy_string(vm, s, utf8_one_char_length(s)));
}

static DaiValue
DaiObjString_subscript_set(__attribute__((unused)) DaiVM* vm, DaiValue receiver, DaiValue index,
                           DaiValue value) {
    DaiObjError* err = DaiObjError_Newf(vm, "'string' object does not support item assignment");
    return OBJ_VAL(err);
}

static int
DaiObjString_equal(DaiValue a, DaiValue b, __attribute__((unused)) int* limit) {
    DaiObjString* sa = AS_STRING(a);
    DaiObjString* sb = AS_STRING(b);
    return (sa == sb) ||
           (sa->length == sb->length && strncmp(sa->chars, sb->chars, sa->length) == 0);
}

static uint64_t
DaiObjString_hash(DaiValue value) {
    return AS_STRING(value)->hash;
}

static struct DaiObjOperation string_operation = {
    .get_property_func  = DaiObjString_get_property,
    .set_property_func  = NULL,
    .subscript_get_func = DaiObjString_subscript_get,
    .subscript_set_func = DaiObjString_subscript_set,
    .string_func        = DaiObjString_String,
    .equal_func         = DaiObjString_equal,
    .hash_func          = DaiObjString_hash,
    .iter_init_func     = NULL,
    .iter_next_func     = NULL,
    .get_method_func    = DaiObjString_get_property,
};


static DaiObjString*
allocate_string(DaiVM* vm, char* chars, int length, uint32_t hash) {
    DaiObjString* string  = ALLOCATE_OBJ(vm, DaiObjString, DaiObjType_string);
    string->length        = length;
    string->utf8_length   = utf8len(chars);
    string->chars         = chars;
    string->hash          = hash;
    string->obj.operation = &string_operation;
    DaiTable_set(&vm->strings, string, NIL_VAL);
    return string;
}
static uint32_t
hash_string(const char* key, int length) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}

DaiObjString*
dai_find_string_intern(DaiVM* vm, const char* chars, int length) {
    uint32_t hash = hash_string(chars, length);
    return DaiTable_findString(&vm->strings, chars, length, hash);
}

DaiObjString*
dai_take_string_intern(DaiVM* vm, char* chars, int length) {
    uint32_t hash          = hash_string(chars, length);
    DaiObjString* interned = DaiTable_findString(&vm->strings, chars, length, hash);
    if (interned != NULL) {
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }

    return allocate_string(vm, chars, length, hash);
}
DaiObjString*
dai_copy_string_intern(DaiVM* vm, const char* chars, int length) {
    uint32_t hash          = hash_string(chars, length);
    DaiObjString* interned = DaiTable_findString(&vm->strings, chars, length, hash);
    if (interned != NULL) return interned;

    char* heap_chars = VM_ALLOCATE(vm, char, length + 1);
    memcpy(heap_chars, chars, length);
    heap_chars[length] = '\0';
    return allocate_string(vm, heap_chars, length, hash);
}

DaiObjString*
dai_take_string(DaiVM* vm, char* chars, int length) {
    uint32_t hash = hash_string(chars, length);
    return allocate_string(vm, chars, length, hash);
}

DaiObjString*
dai_copy_string(DaiVM* vm, const char* chars, int length) {
    uint32_t hash    = hash_string(chars, length);
    char* heap_chars = VM_ALLOCATE(vm, char, length + 1);
    memcpy(heap_chars, chars, length);
    heap_chars[length] = '\0';
    return allocate_string(vm, heap_chars, length, hash);
}

// Function to compare two DaiObjString objects
int
DaiObjString_cmp(DaiObjString* a, DaiObjString* b) {
    // Compare the lengths first
    if (a->length < b->length) return -1;
    if (a->length > b->length) return 1;

    // If lengths are equal, compare the strings lexicographically
    return strcmp(a->chars, b->chars);
}
