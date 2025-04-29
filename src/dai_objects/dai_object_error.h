#ifndef CBDAI_DAI_OBJECT_ERROR_H
#define CBDAI_DAI_OBJECT_ERROR_H

#include "dai_error.h"
#include "dai_objects/dai_object_base.h"
#include "dai_utils.h"

typedef enum {
    DaiError_syntax  = 0,
    DaiError_compile = 1,
    DaiError_runtime = 2,
} DaiErrorKind;

const char*
DaiErrorKind_string(DaiErrorKind kind);

typedef struct {
    DaiObj obj;
    char message[256];
    DaiFilePos pos;
    DaiErrorKind kind;
} DaiObjError;
DaiObjError*
DaiObjError_Newf(DaiVM* vm, const char* format, ...) __attribute__((format(printf, 2, 3)));
DaiObjError*
DaiObjError_From(DaiVM* vm, DaiError* err);


#endif /* CBDAI_DAI_OBJECT_ERROR_H */
