#ifndef SRC_DAI_OBJECT_ERROR_H_
#define SRC_DAI_OBJECT_ERROR_H_

#include "dai_objects/dai_object_base.h"

typedef struct {
    DaiObj obj;
    char message[256];
} DaiObjError;
DaiObjError*
DaiObjError_Newf(DaiVM* vm, const char* format, ...) __attribute__((format(printf, 2, 3)));


#endif /* SRC_DAI_OBJECT_ERROR_H_ */