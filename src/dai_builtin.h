#ifndef SRC_DAI_BUILTIN_H_
#define SRC_DAI_BUILTIN_H_

#include "dai_object.h"

#define BUILTIN_OBJECT_MAX_COUNT 256

typedef struct {
    const char* name;
    DaiValue value;
} DaiBuiltinObject;

DaiBuiltinObject*
init_builtin_objects(DaiVM* vm, int* count);

extern const char* builtin_names[];

#endif /* SRC_DAI_BUILTIN_H_ */