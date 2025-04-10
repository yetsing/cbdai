#include "dai_canvas.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include "dai_object.h"
#include "dai_value.h"
#include "dai_vm.h"

static SDL_Window* window            = NULL;
static SDL_Renderer* renderer        = NULL;
static const char* module_name       = "canvas";
static bool is_running               = false;
static DaiObjModule* canvas_module   = NULL;
static const char* texture_name      = "canvas.Texture";
static const char* rect_struct_name  = "canvas.Rect";
static const char* point_struct_name = "canvas.Point";

#define STRING_NAME(name) dai_copy_string_intern(vm, name, strlen(name))
#define CHECK_INIT()                                                       \
    if (window == NULL) {                                                  \
        DaiObjError* err = DaiObjError_Newf(vm, "canvas not initialized"); \
        return OBJ_VAL(err);                                               \
    }


// #region event handle
#define EVENT_CALLBAK_MAX 10
typedef struct {
    DaiValue callback[EVENT_CALLBAK_MAX];
    int callback_count;
} EventCallbacks;

static DaiObjClass* keyboard_event_class = NULL;
static DaiObjClass* mouse_event_class    = NULL;


static DaiObjTuple* keydown_callbacks     = NULL;
static DaiObjTuple* keyup_callbacks       = NULL;
static DaiObjTuple* mousedown_callbacks   = NULL;
static DaiObjTuple* mouseup_callbacks     = NULL;
static DaiObjTuple* mousemotion_callbacks = NULL;

static DaiValue
convert_sdl_event(DaiVM* vm, SDL_Event event) {
    switch (event.type) {
        case SDL_EVENT_KEY_DOWN:;
        case SDL_EVENT_KEY_UP: {
            // code altKey ctrlKey shiftKey metaKey repeat key
            const char* key_name = SDL_GetKeyName(event.key.key);
            DaiValue value[]     = {
                INTEGER_VAL(event.key.key),
                BOOL_VAL((event.key.mod & SDL_KMOD_ALT) != 0),
                BOOL_VAL((event.key.mod & SDL_KMOD_CTRL) != 0),
                BOOL_VAL((event.key.mod & SDL_KMOD_SHIFT) != 0),
                BOOL_VAL((event.key.mod & SDL_KMOD_GUI) != 0),
                BOOL_VAL(event.key.repeat),
                OBJ_VAL(STRING_NAME(key_name)),
            };
            return DaiObjClass_call(keyboard_event_class, vm, 7, value);
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            // x y button
            DaiValue value[] = {
                FLOAT_VAL(event.button.x),
                FLOAT_VAL(event.button.y),
                INTEGER_VAL(event.button.button),
            };
            return DaiObjClass_call(mouse_event_class, vm, 3, value);
        }
        case SDL_EVENT_MOUSE_MOTION: {
            // x y button
            DaiValue value[] = {
                FLOAT_VAL(event.motion.x),
                FLOAT_VAL(event.motion.y),
                INTEGER_VAL(event.motion.state),
            };
            return DaiObjClass_call(mouse_event_class, vm, 3, value);
        }
        default: return NIL_VAL;
    }
    return NIL_VAL;
}

static DaiObjTuple*
get_event_callbacks(int64_t event_type) {
    switch (event_type) {
        case SDL_EVENT_KEY_DOWN: return keydown_callbacks;
        case SDL_EVENT_KEY_UP: return keyup_callbacks;
        case SDL_EVENT_MOUSE_BUTTON_DOWN: return mousedown_callbacks;
        case SDL_EVENT_MOUSE_BUTTON_UP: return mouseup_callbacks;
        case SDL_EVENT_MOUSE_MOTION: return mousemotion_callbacks;
        default: return NULL;
    }
}

static DaiValue
handle_event(DaiVM* vm) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        DaiValue dai_event = convert_sdl_event(vm, event);
        switch (event.type) {
            case SDL_EVENT_QUIT: {
                is_running = false;
                break;
            }
            case SDL_EVENT_MOUSE_BUTTON_UP:
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_MOTION:
            case SDL_EVENT_KEY_UP:
            case SDL_EVENT_KEY_DOWN: {
                DaiObjTuple* callbacks = get_event_callbacks(event.type);
                assert(callbacks != NULL);
                int count = DaiObjTuple_length(callbacks);
                for (int i = 0; i < count; i++) {
                    DaiValue ret = DaiVM_runCall(vm, DaiObjTuple_get(callbacks, i), 1, dai_event);
                    if (IS_ERROR(ret)) {
                        return ret;
                    }
                }
                break;
            }
            default: break;
        }
    }
    return UNDEFINED_VAL;
}

// #endregion


// #region canvas module functions
// canvas module functions:
//      init(width, height)
//      fillRect(x, y, width, height, r, g, b, a)
//      clear()
//      present()
//      run(callback)
//      quit()
//      addEventListener(event, callback)
//      loadImage(path)
//      drawImage(...)
//      Rect(x, y, width, height)
//      Point(x, y)
//      drawImageEx(image, srcrect, dstrect, angle, center, flip)

static DaiValue
builtin_canvas_init(DaiVM* vm, __attribute__((unused)) DaiValue receiver, int argc,
                    DaiValue* argv) {
    if (argc != 2) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "canvas.init() expected 2 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (!IS_INTEGER(argv[0]) || !IS_INTEGER(argv[1])) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "canvas.init() expected integer arguments, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
    if (window != NULL) {
        DaiObjError* err = DaiObjError_Newf(vm, "canvas.init() already called");
        return OBJ_VAL(err);
    }
    int width  = AS_INTEGER(argv[0]);
    int height = AS_INTEGER(argv[1]);

    window = SDL_CreateWindow("Dai Canvas", width, height, SDL_WINDOW_RESIZABLE);
    if (window == NULL) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "SDL could not create window! SDL_Error: %s", SDL_GetError());
        return OBJ_VAL(err);
    }

    renderer = SDL_CreateRenderer(window, NULL);
    if (renderer == NULL) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "SDL could not create renderer! SDL_Error: %s", SDL_GetError());
        return OBJ_VAL(err);
    }

    DaiObjModule_set_global(canvas_module, "width", INTEGER_VAL(width));
    DaiObjModule_set_global(canvas_module, "height", INTEGER_VAL(height));
    return NIL_VAL;
}

static DaiValue
builtin_canvas_fillRect(DaiVM* vm, __attribute__((unused)) DaiValue receiver, int argc,
                        DaiValue* argv) {
    if (argc != 8) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "canvas.fillRect() expected 8 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    for (int i = 0; i < 8; i++) {
        if (!IS_INTEGER(argv[i])) {
            DaiObjError* err =
                DaiObjError_Newf(vm,
                                 "canvas.fillRect() expected integer arguments, but got %s",
                                 dai_value_ts(argv[i]));
            return OBJ_VAL(err);
        }
    }
    CHECK_INIT();
    int x      = AS_INTEGER(argv[0]);
    int y      = AS_INTEGER(argv[1]);
    int width  = AS_INTEGER(argv[2]);
    int height = AS_INTEGER(argv[3]);
    int r      = AS_INTEGER(argv[4]);
    int g      = AS_INTEGER(argv[5]);
    int b      = AS_INTEGER(argv[6]);
    int a      = AS_INTEGER(argv[7]);

    if (!SDL_SetRenderDrawColor(renderer, r, g, b, a)) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "SDL could not set draw color! SDL_Error: %s", SDL_GetError());
        return OBJ_VAL(err);
    }
    SDL_FRect rect;
    rect.x = x;
    rect.y = y;
    rect.w = width;
    rect.h = height;
    if (!SDL_RenderFillRect(renderer, &rect)) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "SDL could not fill rect! SDL_Error: %s", SDL_GetError());
        return OBJ_VAL(err);
    }

    return NIL_VAL;
}

static DaiValue
builtin_canvas_clear(DaiVM* vm, __attribute__((unused)) DaiValue receiver, int argc,
                     DaiValue* argv) {
    if (argc != 0) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "canvas.clear() expected 0 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    CHECK_INIT();
    if (!SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255)) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "SDL could not set draw color! SDL_Error: %s", SDL_GetError());
        return OBJ_VAL(err);
    }
    if (!SDL_RenderClear(renderer)) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "SDL could not clear window! SDL_Error: %s", SDL_GetError());
        return OBJ_VAL(err);
    }
    return NIL_VAL;
}

static DaiValue
builtin_canvas_present(DaiVM* vm, __attribute__((unused)) DaiValue receiver, int argc,
                       DaiValue* argv) {
    if (argc != 0) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "canvas.present() expected 0 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    CHECK_INIT();
    if (!SDL_RenderPresent(renderer)) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "SDL could not present window! SDL_Error: %s", SDL_GetError());
        return OBJ_VAL(err);
    }
    return NIL_VAL;
}

static DaiValue
builtin_canvas_run(DaiVM* vm, __attribute__((unused)) DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "canvas.run() expected 1 argument, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (!IS_FUNCTION_LIKE(argv[0])) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "canvas.run() expected function argument, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
    DaiValue callback = argv[0];
    CHECK_INIT();
    is_running = true;
    DaiValue ret;
    while (is_running) {
        DaiVM_pauseGC(vm);
        ret = handle_event(vm);
        DaiVM_resumeGC(vm);
        if (IS_ERROR(ret)) {
            return ret;
        }
        ret = DaiVM_runCall(vm, callback, 0);
        if (IS_ERROR(ret)) {
            return ret;
        }
    }
    return NIL_VAL;
}

static DaiValue
builtin_canvas_quit(DaiVM* vm, __attribute__((unused)) DaiValue receiver, int argc,
                    DaiValue* argv) {
    if (argc != 0) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "canvas.quit() expected 0 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    CHECK_INIT();
    is_running = false;
    return NIL_VAL;
}

static DaiValue
builtin_canvas_addEventListener(DaiVM* vm, __attribute__((unused)) DaiValue receiver, int argc,
                                DaiValue* argv) {
    if (argc != 2) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "canvas.addEventListener() expected 2 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (!IS_INTEGER(argv[0])) {
        DaiObjError* err =
            DaiObjError_Newf(vm,
                             "canvas.addEventListener() expected integer argument, but got %s",
                             dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
    if (!IS_FUNCTION_LIKE(argv[1])) {
        DaiObjError* err =
            DaiObjError_Newf(vm,
                             "canvas.addEventListener() expected function argument, but got %s",
                             dai_value_ts(argv[1]));
        return OBJ_VAL(err);
    }
    int64_t event_type = AS_INTEGER(argv[0]);
    int64_t index      = 0;
    switch (event_type) {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
        case SDL_EVENT_MOUSE_MOTION: {
            DaiObjTuple* callbacks = get_event_callbacks(event_type);
            assert(callbacks != NULL);
            DaiObjTuple_append(callbacks, argv[1]);
            index = DaiObjTuple_length(callbacks) - 1;
            break;
        }
        default: {
            DaiObjError* err = DaiObjError_Newf(
                vm, "canvas.addEventListener() unknown event type %" PRId64, event_type);
            return OBJ_VAL(err);
        }
    }

    return INTEGER_VAL(index);
}

static void
texture_destructor(void* ptr) {
    SDL_DestroyTexture((SDL_Texture*)ptr);
}

static DaiValue
builtin_canvas_loadImage(DaiVM* vm, __attribute__((unused)) DaiValue receiver, int argc,
                         DaiValue* argv) {
    if (argc != 1) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "canvas.loadImage() expected 1 argument, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (!IS_STRING(argv[0])) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "canvas.loadImage() expected string argument, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
    CHECK_INIT();
    const char* path     = AS_CSTRING(argv[0]);
    SDL_Texture* texture = IMG_LoadTexture(renderer, path);
    if (texture == NULL) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "SDL could not load image! SDL_Error: %s", SDL_GetError());
        return OBJ_VAL(err);
    }
    DaiObjStruct* obj = DaiObjStruct_New(vm, texture_name, texture, texture_destructor);
    float w, h;
    SDL_GetTextureSize(texture, &w, &h);
    DaiObjStruct_set(vm, obj, "width", INTEGER_VAL(w));
    DaiObjStruct_set(vm, obj, "height", INTEGER_VAL(h));
    return OBJ_VAL(obj);
}

// ref: https://developer.mozilla.org/en-US/docs/Web/API/CanvasRenderingContext2D/drawImage
// drawImage(image, dx, dy)
// drawImage(image, dx, dy, dWidth, dHeight)
// drawImage(image, sx, sy, sWidth, sHeight, dx, dy, dWidth, dHeight)
static DaiValue
builtin_canvas_drawImage(DaiVM* vm, __attribute__((unused)) DaiValue receiver, int argc,
                         DaiValue* argv) {
    if (argc != 3 && argc != 5 && argc != 9) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "canvas.drawImage() expected 3/5/9 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (!IS_STRUCT(argv[0])) {
        DaiObjError* err =
            DaiObjError_Newf(vm,
                             "canvas.drawImage() expected struct %s argument, but got %s",
                             texture_name,
                             dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
    for (int i = 1; i < argc; i++) {
        if (!IS_INTEGER(argv[i])) {
            DaiObjError* err =
                DaiObjError_Newf(vm,
                                 "canvas.drawImage() expected integer arguments, but got %s",
                                 dai_value_ts(argv[i]));
            return OBJ_VAL(err);
        }
    }
    DaiObjStruct* obj = AS_STRUCT(argv[0]);
    if (obj->name != texture_name) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "canvas.drawImage() expected struct %s argument", texture_name);
        return OBJ_VAL(err);
    }
    CHECK_INIT();

    SDL_Texture* texture = (SDL_Texture*)obj->udata;
    float w, h;
    SDL_GetTextureSize(texture, &w, &h);
    SDL_FRect srcrect = {.x = 0, .y = 0, .w = w, .h = h};
    SDL_FRect dstrect = {.x = 0, .y = 0, .w = w, .h = h};
    if (argc == 3) {
        // dx, dy
        dstrect.x = AS_INTEGER(argv[1]);
        dstrect.y = AS_INTEGER(argv[2]);
    } else if (argc == 5) {
        // dx, dy, dWidth, dHeight
        dstrect.x = AS_INTEGER(argv[1]);
        dstrect.y = AS_INTEGER(argv[2]);
        dstrect.w = AS_INTEGER(argv[3]);
        dstrect.h = AS_INTEGER(argv[4]);
    } else if (argc == 9) {
        // sx, sy, sWidth, sHeight, dx, dy, dWidth, dHeight
        srcrect.x = AS_INTEGER(argv[1]);
        srcrect.y = AS_INTEGER(argv[2]);
        srcrect.w = AS_INTEGER(argv[3]);
        srcrect.h = AS_INTEGER(argv[4]);
        dstrect.x = AS_INTEGER(argv[5]);
        dstrect.y = AS_INTEGER(argv[6]);
        dstrect.w = AS_INTEGER(argv[7]);
        dstrect.h = AS_INTEGER(argv[8]);
    }
    if (!SDL_RenderTexture(renderer, texture, &srcrect, &dstrect)) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "SDL could not draw image! SDL_Error: %s", SDL_GetError());
        return OBJ_VAL(err);
    }
    return NIL_VAL;
}

static DaiValue
builtin_canvas_Rect(DaiVM* vm, __attribute__((unused)) DaiValue receiver, int argc,
                    DaiValue* argv) {
    if (argc != 4) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "canvas.Rect() expected 4 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    for (int i = 0; i < 4; i++) {
        if (!IS_INTEGER(argv[i])) {
            DaiObjError* err = DaiObjError_Newf(
                vm, "canvas.Rect() expected integer arguments, but got %s", dai_value_ts(argv[i]));
            return OBJ_VAL(err);
        }
    }
    DaiObjStruct* obj = DaiObjStruct_New(vm, rect_struct_name, NULL, NULL);
    DaiObjStruct_set(vm, obj, "x", argv[0]);
    DaiObjStruct_set(vm, obj, "y", argv[1]);
    DaiObjStruct_set(vm, obj, "width", argv[2]);
    DaiObjStruct_set(vm, obj, "height", argv[3]);
    return OBJ_VAL(obj);
}

static DaiValue
builtin_canvas_Point(DaiVM* vm, __attribute__((unused)) DaiValue receiver, int argc,
                     DaiValue* argv) {
    if (argc != 2) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "canvas.Point() expected 2 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    for (int i = 0; i < 2; i++) {
        if (!IS_INTEGER(argv[i])) {
            DaiObjError* err = DaiObjError_Newf(
                vm, "canvas.Point() expected integer arguments, but got %s", dai_value_ts(argv[i]));
            return OBJ_VAL(err);
        }
    }
    DaiObjStruct* obj = DaiObjStruct_New(vm, point_struct_name, NULL, NULL);
    DaiObjStruct_set(vm, obj, "x", argv[0]);
    DaiObjStruct_set(vm, obj, "y", argv[1]);
    return OBJ_VAL(obj);
}

// drawImageEx(image, srcrect, dstrect, angle, center, flip)
//     angle: int|float 角度
static DaiValue
builtin_canvas_drawImageEx(DaiVM* vm, __attribute__((unused)) DaiValue receiver, int argc,
                           DaiValue* argv) {
    if (argc != 6) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "canvas.drawImageEx() expected 6 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    SDL_Texture* texture = NULL;
    SDL_FRect srcrect;
    SDL_FRect dstrect;
    double angle = 0;
    SDL_FPoint center;
    SDL_FlipMode flip = SDL_FLIP_NONE;

    // #region 验证并且提取参数
    if (!IS_STRUCT(argv[0]) || AS_STRUCT(argv[0])->name != texture_name) {
        DaiObjError* err =
            DaiObjError_Newf(vm,
                             "canvas.drawImageEx() expected struct %s argument, but got %s",
                             texture_name,
                             dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
    DaiObjStruct* obj = NULL;
    texture           = AS_STRUCT(argv[0])->udata;

    if (!IS_STRUCT(argv[1]) || AS_STRUCT(argv[1])->name != rect_struct_name) {
        DaiObjError* err =
            DaiObjError_Newf(vm,
                             "canvas.drawImageEx() expected struct %s argument, but got %s",
                             rect_struct_name,
                             dai_value_ts(argv[1]));
        return OBJ_VAL(err);
    }
    obj             = AS_STRUCT(argv[1]);
    DaiValue xvalue = DaiObjStruct_get(vm, obj, "x");
    if (IS_ERROR(xvalue)) {
        return xvalue;
    }
    DaiValue yvalue = DaiObjStruct_get(vm, obj, "y");
    if (IS_ERROR(yvalue)) {
        return yvalue;
    }
    DaiValue wvalue = DaiObjStruct_get(vm, obj, "width");
    if (IS_ERROR(wvalue)) {
        return wvalue;
    }
    DaiValue hvalue = DaiObjStruct_get(vm, obj, "height");
    if (IS_ERROR(hvalue)) {
        return hvalue;
    }
    srcrect.x = AS_INTEGER(xvalue);
    srcrect.y = AS_INTEGER(yvalue);
    srcrect.w = AS_INTEGER(wvalue);
    srcrect.h = AS_INTEGER(hvalue);

    if (!IS_STRUCT(argv[2]) || AS_STRUCT(argv[2])->name != rect_struct_name) {
        DaiObjError* err =
            DaiObjError_Newf(vm,
                             "canvas.drawImageEx() expected struct %s argument, but got %s",
                             rect_struct_name,
                             dai_value_ts(argv[2]));
        return OBJ_VAL(err);
    }
    obj    = AS_STRUCT(argv[2]);
    xvalue = DaiObjStruct_get(vm, obj, "x");
    if (IS_ERROR(xvalue)) {
        return xvalue;
    }
    yvalue = DaiObjStruct_get(vm, obj, "y");
    if (IS_ERROR(yvalue)) {
        return yvalue;
    }
    wvalue = DaiObjStruct_get(vm, obj, "width");
    if (IS_ERROR(wvalue)) {
        return wvalue;
    }
    hvalue = DaiObjStruct_get(vm, obj, "height");
    if (IS_ERROR(hvalue)) {
        return hvalue;
    }
    dstrect.x = AS_INTEGER(xvalue);
    dstrect.y = AS_INTEGER(yvalue);
    dstrect.w = AS_INTEGER(wvalue);
    dstrect.h = AS_INTEGER(hvalue);

    if (!IS_INTEGER(argv[3]) && !IS_FLOAT(argv[3])) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "canvas.drawImageEx() expected number argument, but got %s", dai_value_ts(argv[3]));
        return OBJ_VAL(err);
    }
    if (IS_INTEGER(argv[3])) {
        angle = AS_INTEGER(argv[3]);
    } else {
        angle = AS_FLOAT(argv[3]);
    }

    if (!IS_STRUCT(argv[4]) || AS_STRUCT(argv[4])->name != point_struct_name) {
        DaiObjError* err =
            DaiObjError_Newf(vm,
                             "canvas.drawImageEx() expected struct %s argument, but got %s",
                             point_struct_name,
                             dai_value_ts(argv[4]));
        return OBJ_VAL(err);
    }
    obj    = AS_STRUCT(argv[4]);
    xvalue = DaiObjStruct_get(vm, obj, "x");
    if (IS_ERROR(xvalue)) {
        return xvalue;
    }
    yvalue = DaiObjStruct_get(vm, obj, "y");
    if (IS_ERROR(yvalue)) {
        return yvalue;
    }
    center.x = AS_INTEGER(xvalue);
    center.y = AS_INTEGER(yvalue);

    if (!IS_INTEGER(argv[5])) {
        DaiObjError* err =
            DaiObjError_Newf(vm,
                             "canvas.drawImageEx() expected integer argument, but got %s",
                             dai_value_ts(argv[5]));
        return OBJ_VAL(err);
    }
    flip = (SDL_FlipMode)AS_INTEGER(argv[5]);
    if (flip < 0 || flip > 2) {
        DaiObjError* err = DaiObjError_Newf(vm, "canvas.drawImageEx() invalid flip %d", flip);
        return OBJ_VAL(err);
    }
    // #endregion
    CHECK_INIT();
    if (!SDL_RenderTextureRotated(renderer, texture, &srcrect, &dstrect, angle, &center, flip)) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "SDL could not drawEx image! SDL_Error: %s", SDL_GetError());
        return OBJ_VAL(err);
    }
    return NIL_VAL;
}


static DaiObjBuiltinFunction canvas_funcs[] = {
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "init",
        .function = builtin_canvas_init,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "fillRect",
        .function = builtin_canvas_fillRect,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "clear",
        .function = builtin_canvas_clear,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "present",
        .function = builtin_canvas_present,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "run",
        .function = builtin_canvas_run,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "quit",
        .function = builtin_canvas_quit,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "addEventListener",
        .function = builtin_canvas_addEventListener,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "loadImage",
        .function = builtin_canvas_loadImage,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "drawImage",
        .function = builtin_canvas_drawImage,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "Rect",
        .function = builtin_canvas_Rect,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "Point",
        .function = builtin_canvas_Point,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "drawImageEx",
        .function = builtin_canvas_drawImageEx,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name = NULL,
    },
};

// #endregion

static DaiObjModule*
create_canvas_module(DaiVM* vm) {

    DaiObjModule* module = DaiObjModule_New(vm, strdup(module_name), strdup("<builtin>"));
    canvas_module        = module;

    DaiObjModule_add_global(module, "EventKeyDown", INTEGER_VAL(SDL_EVENT_KEY_DOWN));
    DaiObjModule_add_global(module, "EventKeyUp", INTEGER_VAL(SDL_EVENT_KEY_UP));
    DaiObjModule_add_global(module, "EventMouseDown", INTEGER_VAL(SDL_EVENT_MOUSE_BUTTON_DOWN));
    DaiObjModule_add_global(module, "EventMouseUp", INTEGER_VAL(SDL_EVENT_MOUSE_BUTTON_UP));
    DaiObjModule_add_global(module, "EventMouseMotion", INTEGER_VAL(SDL_EVENT_MOUSE_MOTION));

    DaiObjModule_add_global(module, "width", INTEGER_VAL(0));
    DaiObjModule_add_global(module, "height", INTEGER_VAL(0));

    for (int i = 0; canvas_funcs[i].name != NULL; i++) {
        DaiObjModule_add_global(module, canvas_funcs[i].name, OBJ_VAL(&canvas_funcs[i]));
    }

    // create keyboard event class
    keyboard_event_class = DaiObjClass_New(vm, STRING_NAME("KeyboardEvent"));
    DaiObjClass_define_field(
        keyboard_event_class, STRING_NAME("code"), INTEGER_VAL(SDLK_UNKNOWN), true);
    DaiObjClass_define_field(keyboard_event_class, STRING_NAME("altKey"), dai_false, true);
    DaiObjClass_define_field(keyboard_event_class, STRING_NAME("ctrlKey"), dai_false, true);
    DaiObjClass_define_field(keyboard_event_class, STRING_NAME("shiftKey"), dai_false, true);
    DaiObjClass_define_field(keyboard_event_class, STRING_NAME("metaKey"), dai_false, true);
    DaiObjClass_define_field(keyboard_event_class, STRING_NAME("repeat"), dai_false, true);
    DaiObjClass_define_field(
        keyboard_event_class, STRING_NAME("key"), INTEGER_VAL(SDLK_UNKNOWN), true);
    DaiObjModule_add_global(module, "KeyboardEvent", OBJ_VAL(keyboard_event_class));

    // create mouse event class
    mouse_event_class = DaiObjClass_New(vm, STRING_NAME("MouseEvent"));
    DaiObjClass_define_field(mouse_event_class, STRING_NAME("x"), INTEGER_VAL(0), true);
    DaiObjClass_define_field(mouse_event_class, STRING_NAME("y"), INTEGER_VAL(0), true);
    DaiObjClass_define_field(mouse_event_class, STRING_NAME("button"), INTEGER_VAL(0), true);
    DaiObjModule_add_global(module, "MouseEvent", OBJ_VAL(mouse_event_class));

    // 让模块引用事件回调，以防止被垃圾回收
    // reference event callbacks
    keydown_callbacks     = DaiObjTuple_New(vm);
    keyup_callbacks       = DaiObjTuple_New(vm);
    mousedown_callbacks   = DaiObjTuple_New(vm);
    mouseup_callbacks     = DaiObjTuple_New(vm);
    mousemotion_callbacks = DaiObjTuple_New(vm);
    DaiObjModule_add_global(module, "_KeydownCallbacks", OBJ_VAL(keydown_callbacks));
    DaiObjModule_add_global(module, "_KeyupCallbacks", OBJ_VAL(keyup_callbacks));
    DaiObjModule_add_global(module, "_MousedownCallbacks", OBJ_VAL(mousedown_callbacks));
    DaiObjModule_add_global(module, "_MouseupCallbacks", OBJ_VAL(mouseup_callbacks));
    DaiObjModule_add_global(module, "_MousemotionCallbacks", OBJ_VAL(mousemotion_callbacks));
    return module;
}


int
dai_canvas_init(DaiVM* vm) {
    if (!SDL_SetAppMetadata("Dai Canvas", "0.0.1", "com.dai.canvas")) {
        fprintf(stderr, "SDL could not set app metadata!\n");
        return 1;
    }
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr,
                "SDL could not be initialized!\n"
                "SDL_Error: %s\n",
                SDL_GetError());
        return 1;
    }

    DaiObjModule* module = create_canvas_module(vm);
    DaiVM_addBuiltin(vm, module_name, OBJ_VAL(module));
    return 0;
}

void
dai_canvas_quit(DaiVM* vm) {
    if (renderer != NULL) {
        SDL_DestroyRenderer(renderer);
        renderer = NULL;
    }
    if (window != NULL) {
        SDL_DestroyWindow(window);
        window = NULL;
    }
    SDL_Quit();
}