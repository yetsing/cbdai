#ifndef CBDAI_DAI_OBJECT_H
#define CBDAI_DAI_OBJECT_H

// IWYU pragma: begin_exports
#include "dai_objects/dai_object_array.h"
#include "dai_objects/dai_object_base.h"
#include "dai_objects/dai_object_class.h"
#include "dai_objects/dai_object_error.h"
#include "dai_objects/dai_object_function.h"
#include "dai_objects/dai_object_map.h"
#include "dai_objects/dai_object_module.h"
#include "dai_objects/dai_object_range.h"
#include "dai_objects/dai_object_string.h"
#include "dai_objects/dai_object_struct.h"
#include "dai_objects/dai_object_tuple.h"
#include "dai_value.h"
// IWYU pragma: end_exports

const char*
dai_object_ts(DaiValue value);
#endif /* CBDAI_DAI_OBJECT_H */
