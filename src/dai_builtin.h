#ifndef CBDAI_DAI_BUILTIN_H
#define CBDAI_DAI_BUILTIN_H

#include "dai_object.h"

#define BUILTIN_OBJECT_MAX_COUNT 256

typedef struct {
    const char* name;
    DaiValue value;
} DaiBuiltinObject;

DaiBuiltinObject*
init_builtin_objects(DaiVM* vm, int* count);

extern const char* builtin_names[];

#endif /* CBDAI_DAI_BUILTIN_H */
