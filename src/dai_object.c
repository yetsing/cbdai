#include "dai_object.h"


const char*
dai_object_ts(DaiValue value) {
    switch (OBJ_TYPE(value)) {
        case DaiObjType_boundMethod: {
            return "bound_method";
        }
        case DaiObjType_instance: {
            return AS_INSTANCE(value)->klass->name->chars;
        }
        case DaiObjType_class: {
            return "class";
        }
        case DaiObjType_function: return "function";
        case DaiObjType_string: return "string";
        case DaiObjType_builtinFn: return "builtin-function";
        case DaiObjType_closure: {
            return "function";
        }
        case DaiObjType_array: return "array";
        case DaiObjType_arrayIterator: return "array_iterator";
        case DaiObjType_map: return "map";
        case DaiObjType_mapIterator: return "map_iterator";
        case DaiObjType_rangeIterator: return "range_iterator";
        case DaiObjType_error: return "error";
        case DaiObjType_cFunction: return "c-function";
        case DaiObjType_module: return "module";
        case DaiObjType_tuple: return "tuple";
        case DaiObjType_struct: return "struct";
    }
    return "unknown";
}
