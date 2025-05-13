#include "dai_canvas.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include "dai_object.h"
#include "dai_objects/dai_object_base.h"
#include "dai_objects/dai_object_error.h"
#include "dai_objects/dai_object_string.h"
#include "dai_objects/dai_object_struct.h"
#include "dai_value.h"
#include "dai_vm.h"
#include "dai_windows.h"   // IWYU pragma: keep

static SDL_Window* window                     = NULL;
static SDL_Renderer* renderer                 = NULL;
static const char* module_name                = "canvas";
static bool is_running                        = false;
static DaiObjModule* canvas_module            = NULL;
static const char* texture_name               = "canvas.Texture";
static const char* rect_struct_name           = "canvas.Rect";
static const char* point_struct_name          = "canvas.Point";
static const char* keyboard_event_struct_name = "canvas.KeyboardEvent";
static const char* mouse_event_struct_name    = "canvas.MouseEvent";
static bool init_done                         = false;

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

static DaiObjTuple* keydown_callbacks     = NULL;
static DaiObjTuple* keyup_callbacks       = NULL;
static DaiObjTuple* mousedown_callbacks   = NULL;
static DaiObjTuple* mouseup_callbacks     = NULL;
static DaiObjTuple* mousemotion_callbacks = NULL;

typedef struct {
    DAI_OBJ_STRUCT_BASE
    int64_t code;
    bool alt_key;
    bool ctrl_key;
    bool shift_key;
    bool meta_key;
    bool repeat;
    const char* key;
} KeyboardEventStruct;
static DaiValue
KeyboardEventStruct_get_property(DaiVM* vm, DaiValue receiver, DaiObjString* name) {
    KeyboardEventStruct* kb = (KeyboardEventStruct*)AS_STRUCT(receiver);
    char* cname             = name->chars;
    if (strcmp(cname, "code") == 0) {
        return INTEGER_VAL(kb->code);
    }
    if (strcmp(cname, "key") == 0) {
        DaiObjString* k = dai_copy_string_intern(vm, kb->key, strlen(kb->key));
        return OBJ_VAL(k);
    }
    if (strcmp(cname, "altKey") == 0) {
        return BOOL_VAL(kb->alt_key);
    }
    if (strcmp(cname, "ctrlKey") == 0) {
        return BOOL_VAL(kb->ctrl_key);
    }
    if (strcmp(cname, "shiftKey") == 0) {
        return BOOL_VAL(kb->shift_key);
    }
    if (strcmp(cname, "metaKey") == 0) {
        return BOOL_VAL(kb->meta_key);
    }
    if (strcmp(cname, "repeat") == 0) {
        return BOOL_VAL(kb->repeat);
    }
    DaiObjError* err = DaiObjError_Newf(vm, "'KeyboardEvent' has not property '%s'", cname);
    return OBJ_VAL(err);
}

static struct DaiObjOperation keyboard_event_operation = {
    .get_property_func  = KeyboardEventStruct_get_property,
    .set_property_func  = NULL,
    .subscript_get_func = NULL,
    .subscript_set_func = NULL,
    .string_func        = dai_default_string_func,
    .equal_func         = dai_default_equal,
    .hash_func          = dai_default_hash,
    .iter_init_func     = NULL,
    .iter_next_func     = NULL,
    .get_method_func    = NULL,
};

typedef struct {
    DAI_OBJ_STRUCT_BASE
    float x;
    float y;
    int button;
} MouseEventStruct;
static DaiValue
MouseEventStruct_get_property(DaiVM* vm, DaiValue receiver, DaiObjString* name) {
    MouseEventStruct* event = (MouseEventStruct*)AS_STRUCT(receiver);
    char* cname             = name->chars;
    if (strcmp(cname, "x") == 0) {
        return FLOAT_VAL(event->x);
    }
    if (strcmp(cname, "y") == 0) {
        return FLOAT_VAL(event->y);
    }
    if (strcmp(cname, "button") == 0) {
        return INTEGER_VAL(event->button);
    }
    DaiObjError* err = DaiObjError_Newf(vm, "'MouseEvent' has not property '%s'", cname);
    return OBJ_VAL(err);
}

static struct DaiObjOperation mouse_event_operation = {
    .get_property_func  = MouseEventStruct_get_property,
    .set_property_func  = NULL,
    .subscript_get_func = NULL,
    .subscript_set_func = NULL,
    .string_func        = dai_default_string_func,
    .equal_func         = dai_default_equal,
    .hash_func          = dai_default_hash,
    .iter_init_func     = NULL,
    .iter_next_func     = NULL,
    .get_method_func    = NULL,
};

static DaiValue
convert_sdl_event(DaiVM* vm, SDL_Event event) {
    switch (event.type) {
        case SDL_EVENT_KEY_DOWN:;
        case SDL_EVENT_KEY_UP: {
            // code altKey ctrlKey shiftKey metaKey repeat key
            const char* key_name = SDL_GetKeyName(event.key.key);
            KeyboardEventStruct* kb =
                (KeyboardEventStruct*)DaiObjStruct_New(vm,
                                                       keyboard_event_struct_name,
                                                       &keyboard_event_operation,
                                                       sizeof(KeyboardEventStruct),
                                                       NULL);
            kb->code      = event.key.key;
            kb->alt_key   = (event.key.mod & SDL_KMOD_ALT) != 0;
            kb->ctrl_key  = (event.key.mod & SDL_KMOD_CTRL) != 0;
            kb->shift_key = (event.key.mod & SDL_KMOD_SHIFT) != 0;
            kb->meta_key  = (event.key.mod & SDL_KMOD_GUI) != 0;
            kb->repeat    = event.key.repeat;
            kb->key       = key_name;
            return OBJ_VAL(kb);
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            // x y button
            MouseEventStruct* m = (MouseEventStruct*)DaiObjStruct_New(vm,
                                                                      mouse_event_struct_name,
                                                                      &mouse_event_operation,
                                                                      sizeof(MouseEventStruct),
                                                                      NULL);
            m->x                = event.button.x;
            m->y                = event.button.y;
            m->button           = event.button.button;
            return OBJ_VAL(m);
        }
        case SDL_EVENT_MOUSE_MOTION: {
            // x y button
            MouseEventStruct* m = (MouseEventStruct*)DaiObjStruct_New(vm,
                                                                      mouse_event_struct_name,
                                                                      &mouse_event_operation,
                                                                      sizeof(MouseEventStruct),
                                                                      NULL);
            m->x                = event.button.x;
            m->y                = event.button.y;
            m->button           = event.button.button;
            return OBJ_VAL(m);
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
                    if (DAI_IS_ERROR(ret)) {
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
//      run(ms, callback)
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
    if (init_done || window != NULL) {
        DaiObjError* err = DaiObjError_Newf(vm, "canvas.init() already called");
        return OBJ_VAL(err);
    }
    if (!SDL_SetAppMetadata("Dai Canvas", "0.0.1", "com.dai.canvas")) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "SDL could not set app metadata! SDL_Error: %s", SDL_GetError());
        return OBJ_VAL(err);
    }
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "SDL could not be initialized! SDL_Error: %s", SDL_GetError());
        return OBJ_VAL(err);
    }
    init_done = true;

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
    if (argc != 2) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "canvas.run() expected 2 argument, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (!IS_INTEGER(argv[0]) && !IS_FLOAT(argv[0])) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "canvas.run() expected integer/float argument, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
    if (!IS_FUNCTION_LIKE(argv[1])) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "canvas.run() expected function argument, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
    DaiValue callback = argv[1];
    CHECK_INIT();
    is_running = true;
    DaiValue ret;
    uint64_t interval = 0;

    // Get the interval value from argv[0]
    if (IS_INTEGER(argv[0])) {
        interval = AS_INTEGER(argv[0]);
    } else if (IS_FLOAT(argv[0])) {
        interval = AS_FLOAT(argv[0]);
    }

    while (is_running) {
        uint64_t start_time = SDL_GetTicks();

        DaiVM_pauseGC(vm);
        ret = handle_event(vm);
        DaiVM_resumeGC(vm);
        if (DAI_IS_ERROR(ret)) {
            return ret;
        }
        ret = DaiVM_runCall(vm, callback, 0);
        if (DAI_IS_ERROR(ret)) {
            return ret;
        }
        if (!SDL_RenderPresent(renderer)) {
            DaiObjError* err =
                DaiObjError_Newf(vm, "SDL could not present window! SDL_Error: %s", SDL_GetError());
            return OBJ_VAL(err);
        }

        // Calculate how much time to delay
        uint64_t elapsed = SDL_GetTicks() - start_time;
        if (elapsed < interval) {
            SDL_Delay(interval - elapsed);
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

typedef struct {
    DAI_OBJ_STRUCT_BASE
    SDL_Texture* texture;
    DaiValue width;
    DaiValue height;
} TextureStruct;
static DaiValue
TextureStruct_get_property(DaiVM* vm, DaiValue receiver, DaiObjString* name) {
    char* cname        = name->chars;
    TextureStruct* obj = (TextureStruct*)AS_STRUCT(receiver);
    if (strcmp(cname, "width") == 0) {
        return obj->width;
    } else if (strcmp(cname, "height") == 0) {
        return obj->height;
    } else {
        DaiObjError* err = DaiObjError_Newf(
            vm, "'%s' object has not property '%s'", dai_value_ts(receiver), name->chars);
        return OBJ_VAL(err);
    }
}
static char*
TextureStruct_String(DaiValue value, __attribute__((unused)) DaiPtrArray* visited) {
    char buf[128];
    TextureStruct* obj = (TextureStruct*)AS_STRUCT(value);
    int length         = snprintf(buf, sizeof(buf), "<struct %s(%p)>", obj->name, obj->texture);
    return strndup(buf, length);
}
static struct DaiObjOperation texture_struct_operation = {
    .get_property_func  = TextureStruct_get_property,
    .set_property_func  = NULL,
    .subscript_get_func = NULL,
    .subscript_set_func = NULL,
    .string_func        = TextureStruct_String,
    .equal_func         = dai_default_equal,
    .hash_func          = dai_default_hash,
    .iter_init_func     = NULL,
    .iter_next_func     = NULL,
    .get_method_func    = NULL,
};
static void
texture_destructor(DaiObjStruct* st) {
    TextureStruct* tex_obj = (TextureStruct*)st;
    SDL_DestroyTexture(tex_obj->texture);
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
    DaiObjStruct* obj = DaiObjStruct_New(
        vm, texture_name, &texture_struct_operation, sizeof(TextureStruct), texture_destructor);
    TextureStruct* tex_obj = (TextureStruct*)obj;
    float w, h;
    SDL_GetTextureSize(texture, &w, &h);
    tex_obj->texture = texture;
    tex_obj->width   = INTEGER_VAL(w);
    tex_obj->height  = INTEGER_VAL(h);
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
    if (!(IS_STRUCT(argv[0]) && AS_STRUCT(argv[0])->name == texture_name)) {
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
    TextureStruct* obj = (TextureStruct*)AS_STRUCT(argv[0]);
    CHECK_INIT();

    SDL_Texture* texture = (SDL_Texture*)obj->texture;
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

typedef struct {
    DAI_OBJ_STRUCT_BASE
    int x;
    int y;
    int width;
    int height;
} RectStruct;
static char*
RectStruct_String(DaiValue value, __attribute__((unused)) DaiPtrArray* visited) {
    char buf[128];
    RectStruct* obj = (RectStruct*)AS_STRUCT(value);
    int length      = snprintf(buf,
                          sizeof(buf),
                          "<struct %s(x=%d, y=%d, width=%d, height=%d)>",
                          obj->name,
                          obj->x,
                          obj->y,
                          obj->width,
                          obj->height);
    return strndup(buf, length);
}

static struct DaiObjOperation rect_struct_operation = {
    .get_property_func  = NULL,
    .set_property_func  = NULL,
    .subscript_get_func = NULL,
    .subscript_set_func = NULL,
    .string_func        = RectStruct_String,
    .equal_func         = dai_default_equal,
    .hash_func          = dai_default_hash,
    .iter_init_func     = NULL,
    .iter_next_func     = NULL,
    .get_method_func    = NULL,
};

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
    DaiObjStruct* obj =
        DaiObjStruct_New(vm, rect_struct_name, &rect_struct_operation, sizeof(RectStruct), NULL);
    RectStruct* rect = (RectStruct*)obj;
    rect->x          = AS_INTEGER(argv[0]);
    rect->y          = AS_INTEGER(argv[1]);
    rect->width      = AS_INTEGER(argv[2]);
    rect->height     = AS_INTEGER(argv[3]);
    return OBJ_VAL(obj);
}

typedef struct {
    DAI_OBJ_STRUCT_BASE
    int x;
    int y;
} PointStruct;

static char*
PointStruct_String(DaiValue value, __attribute__((unused)) DaiPtrArray* visited) {
    char buf[128];
    PointStruct* obj = (PointStruct*)AS_STRUCT(value);
    int length = snprintf(buf, sizeof(buf), "<struct %s(x=%d, y=%d)>", obj->name, obj->x, obj->y);
    return strndup(buf, length);
}

static struct DaiObjOperation point_struct_operation = {
    .get_property_func  = NULL,
    .set_property_func  = NULL,
    .subscript_get_func = NULL,
    .subscript_set_func = NULL,
    .string_func        = PointStruct_String,
    .equal_func         = dai_default_equal,
    .hash_func          = dai_default_hash,
    .iter_init_func     = NULL,
    .iter_next_func     = NULL,
    .get_method_func    = NULL,
};

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
    PointStruct* obj = (PointStruct*)DaiObjStruct_New(
        vm, point_struct_name, &point_struct_operation, sizeof(PointStruct), NULL);
    obj->x = AS_INTEGER(argv[0]);
    obj->y = AS_INTEGER(argv[1]);
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
    TextureStruct* tex_obj = (TextureStruct*)AS_STRUCT(argv[0]);
    texture                = tex_obj->texture;

    if (!IS_STRUCT(argv[1]) || AS_STRUCT(argv[1])->name != rect_struct_name) {
        DaiObjError* err =
            DaiObjError_Newf(vm,
                             "canvas.drawImageEx() expected struct %s argument, but got %s",
                             rect_struct_name,
                             dai_value_ts(argv[1]));
        return OBJ_VAL(err);
    }
    RectStruct* rect_obj = (RectStruct*)AS_STRUCT(argv[1]);
    srcrect.x            = rect_obj->x;
    srcrect.y            = rect_obj->y;
    srcrect.w            = rect_obj->width;
    srcrect.h            = rect_obj->height;

    if (!IS_STRUCT(argv[2]) || AS_STRUCT(argv[2])->name != rect_struct_name) {
        DaiObjError* err =
            DaiObjError_Newf(vm,
                             "canvas.drawImageEx() expected struct %s argument, but got %s",
                             rect_struct_name,
                             dai_value_ts(argv[2]));
        return OBJ_VAL(err);
    }
    rect_obj  = (RectStruct*)AS_STRUCT(argv[2]);
    dstrect.x = rect_obj->x;
    dstrect.y = rect_obj->y;
    dstrect.w = rect_obj->width;
    dstrect.h = rect_obj->height;

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
    PointStruct* point_obj = (PointStruct*)AS_STRUCT(argv[4]);
    center.x               = (float)point_obj->x;
    center.y               = (float)point_obj->y;

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
    DaiObjModule* module = create_canvas_module(vm);
    DaiVM_addBuiltin(vm, module_name, OBJ_VAL(module));
    return 0;
}

void
dai_canvas_quit(DaiVM* vm) {
    if (!init_done) {
        return;
    }
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