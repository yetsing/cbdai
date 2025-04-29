#ifndef CBDAI_DAI_OBJECT_STRING_H
#define CBDAI_DAI_OBJECT_STRING_H

#include "dai_objects/dai_object_base.h"

struct DaiObjString {
    DaiObj obj;
    int length;   // bytes length
    int utf8_length;
    char* chars;
    uint32_t hash;
};

DaiObjString*
dai_find_string_intern(DaiVM* vm, const char* chars, int length);
DaiObjString*
dai_take_string_intern(DaiVM* vm, char* chars, int length);
DaiObjString*
dai_copy_string_intern(DaiVM* vm, const char* chars, int length);
DaiObjString*
dai_take_string(DaiVM* vm, char* chars, int length);
DaiObjString*
dai_copy_string(DaiVM* vm, const char* chars, int length);
int
DaiObjString_cmp(DaiObjString* s1, DaiObjString* s2);

#endif /* CBDAI_DAI_OBJECT_STRING_H */
