#include "dai_canvas.h"

#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <plutovg.h>

#include "dai_canvas_keycode.h"
#include "dai_object.h"
#include "dai_objects/dai_object_struct.h"
#include "dai_objects/dai_object_tuple.h"
#include "dai_value.h"
#include "dai_vm.h"
#include "dai_windows.h"   // IWYU pragma: keep

#define CANVAS_MODULE_NAME "Canvas"

static const char* module_name                = CANVAS_MODULE_NAME;
static DaiObjModule* canvas_module            = NULL;
static const char* canvas_name                = CANVAS_MODULE_NAME "Canvas";
static const char* texture_name               = CANVAS_MODULE_NAME "Texture";
static const char* rect_struct_name           = CANVAS_MODULE_NAME "Rect";
static const char* point_struct_name          = CANVAS_MODULE_NAME "Point";
static const char* keyboard_event_struct_name = CANVAS_MODULE_NAME "KeyboardEvent";
static const char* mouse_event_struct_name    = CANVAS_MODULE_NAME "MouseEvent";
static const char* keycode_struct_name        = CANVAS_MODULE_NAME "Keycode";
static const char* mousebutton_struct_name    = CANVAS_MODULE_NAME "MouseButton";
static const char* flipmode_struct_name       = CANVAS_MODULE_NAME "FlipMode";
static bool init_done                         = false;


#define STRING_NAME(name) dai_copy_string_intern(vm, name, strlen(name))
#define CHECK_INIT()                                                                    \
    if (window == NULL) {                                                               \
        DaiObjError* err = DaiObjError_Newf(vm, CANVAS_MODULE_NAME " not initialized"); \
        return OBJ_VAL(err);                                                            \
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

// #endregion

// #region struct utils

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
texture_destructor(DaiVM* vm, DaiObjStruct* st) {
    TextureStruct* tex_obj = (TextureStruct*)st;
    SDL_DestroyTexture(tex_obj->texture);
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



typedef struct {
    DAI_OBJ_STRUCT_BASE
    float x;
    float y;
} PointStruct;

static char*
PointStruct_String(DaiValue value, __attribute__((unused)) DaiPtrArray* visited) {
    char buf[128];
    PointStruct* obj = (PointStruct*)AS_STRUCT(value);
    int length = snprintf(buf, sizeof(buf), "<struct %s(x=%f, y=%f)>", obj->name, obj->x, obj->y);
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

// #endregion

// #region canvas struct

typedef struct {
    SDL_Color color;
} Style;

typedef struct {
    DAI_OBJ_STRUCT_BASE
    int width;
    int height;
    Style fill_style;

    SDL_Window* window;
    SDL_Renderer* renderer;

    bool running;

    DaiObjTuple* keydown_callbacks;
    DaiObjTuple* keyup_callbacks;
    DaiObjTuple* mousedown_callbacks;
    DaiObjTuple* mouseup_callbacks;
    DaiObjTuple* mousemotion_callbacks;
} CanvasStruct;

// #region canvas event handle

static DaiObjTuple*
Canvas_get_event_callbacks(CanvasStruct* canvas, int64_t event_type) {
    switch (event_type) {
        case SDL_EVENT_KEY_DOWN: return canvas->keydown_callbacks;
        case SDL_EVENT_KEY_UP: return canvas->keyup_callbacks;
        case SDL_EVENT_MOUSE_BUTTON_DOWN: return canvas->mousedown_callbacks;
        case SDL_EVENT_MOUSE_BUTTON_UP: return canvas->mouseup_callbacks;
        case SDL_EVENT_MOUSE_MOTION: return canvas->mousemotion_callbacks;
        default: return NULL;
    }
}

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

static DaiValue
Canvas_handle_event(DaiVM* vm, CanvasStruct* canvas) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        DaiValue dai_event = convert_sdl_event(vm, event);
        switch (event.type) {
            case SDL_EVENT_QUIT: {
                canvas->running = false;
                break;
            }
            case SDL_EVENT_MOUSE_BUTTON_UP:
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_MOTION:
            case SDL_EVENT_KEY_UP:
            case SDL_EVENT_KEY_DOWN: {
                DaiObjTuple* callbacks = Canvas_get_event_callbacks(canvas, event.type);
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


// #region canvas struct methods

// setFillColor(r, g, b)
// setFillColor(r, g, b, a)
static DaiValue
Canvas_setFillColor(DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 3 && argc != 4) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "canvas.setFillColor() expected 3 or 4 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    float nums[4] = {0, 0, 0, 255};
    for (int i = 0; i < argc; i++) {
        if (!IS_NUMBER(argv[i])) {
            DaiObjError* err =
                DaiObjError_Newf(vm,
                                 "canvas.setFillColor() expected number arguments, but got %s",
                                 dai_value_ts(argv[i]));
            return OBJ_VAL(err);
        }
        nums[i] = AS_NUMBER(argv[i]);
        if (nums[i] < 0 || nums[i] > 255) {
            DaiObjError* err = DaiObjError_Newf(
                vm, "canvas.setFillColor() expected number between 0 and 255, but got %f", nums[i]);
            return OBJ_VAL(err);
        }
    }
    CanvasStruct* canvas     = (CanvasStruct*)AS_STRUCT(receiver);
    SDL_Color color          = (SDL_Color){.r = nums[0], .g = nums[1], .b = nums[2], .a = nums[3]};
    canvas->fill_style.color = color;
    if (!SDL_SetRenderDrawColor(canvas->renderer, color.r, color.g, color.b, color.a)) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "SDL could not set draw color! SDL_Error: %s", SDL_GetError());
        return OBJ_VAL(err);
    }
    return receiver;
}

// fillRect(x, y, width, height)
static DaiValue
Canvas_fillRect(DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 4) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "canvas.fillRect() expected 4 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    for (int i = 0; i < argc; i++) {
        if (!IS_NUMBER(argv[i])) {
            DaiObjError* err =
                DaiObjError_Newf(vm,
                                 "canvas.fillRect() expected number arguments, but got %s",
                                 dai_value_ts(argv[i]));
            return OBJ_VAL(err);
        }
    }
    float x      = AS_NUMBER(argv[0]);
    float y      = AS_NUMBER(argv[1]);
    float width  = AS_NUMBER(argv[2]);
    float height = AS_NUMBER(argv[3]);

    CanvasStruct* canvas = (CanvasStruct*)AS_STRUCT(receiver);
    SDL_FRect rect       = (SDL_FRect){
              .x = x,
              .y = y,
              .w = width,
              .h = height,
    };
    if (!SDL_RenderFillRect(canvas->renderer, &rect)) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "SDL could not fill rect! SDL_Error: %s", SDL_GetError());
        return OBJ_VAL(err);
    }
    return receiver;
}

// run(ms, callback)
static DaiValue
Canvas_run(DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 2) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "canvas.run() expected 2 argument, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (!IS_NUMBER(argv[0])) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "canvas.run() expected number argument, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
    if (!IS_FUNCTION_LIKE(argv[1])) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "canvas.run() expected function argument, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
    CanvasStruct* canvas = (CanvasStruct*)AS_STRUCT(receiver);
    DaiValue callback    = argv[1];
    canvas->running      = true;
    DaiValue ret;
    uint64_t interval = AS_NUMBER(argv[0]);

    SDL_Renderer* renderer = canvas->renderer;
    while (canvas->running) {
        uint64_t start_time = SDL_GetTicks();

        DaiVM_pauseGC(vm);
        ret = Canvas_handle_event(vm, canvas);
        DaiVM_resumeGC(vm);
        if (DAI_IS_ERROR(ret)) {
            return ret;
        }
        ret = DaiVM_runCall(vm, callback, 1, receiver);
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
    return receiver;
}

// clear()
static DaiValue
Canvas_clear(DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 0) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "canvas.clear() expected 0 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    CanvasStruct* canvas   = (CanvasStruct*)AS_STRUCT(receiver);
    SDL_Renderer* renderer = canvas->renderer;
    if (!SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0)) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "SDL could not set draw color! SDL_Error: %s", SDL_GetError());
        return OBJ_VAL(err);
    }
    if (!SDL_RenderClear(renderer)) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "SDL could not clear window! SDL_Error: %s", SDL_GetError());
        return OBJ_VAL(err);
    }
    return receiver;
}

// addEventListener(event, callback)
//      callback(event)
static DaiValue
Canvas_addEventListener(DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
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
    CanvasStruct* canvas = (CanvasStruct*)AS_STRUCT(receiver);
    int64_t event_type   = AS_INTEGER(argv[0]);
    int64_t index        = 0;
    switch (event_type) {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
        case SDL_EVENT_MOUSE_MOTION: {
            DaiObjTuple* callbacks = Canvas_get_event_callbacks(canvas, event_type);
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

// quit()
static DaiValue
Canvas_quit(DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 0) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "canvas.quit() expected 0 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    CanvasStruct* canvas = (CanvasStruct*)AS_STRUCT(receiver);
    canvas->running      = false;
    return receiver;
}

// ref: https://developer.mozilla.org/en-US/docs/Web/API/CanvasRenderingContext2D/drawImage
// drawImage(image, dx, dy)
// drawImage(image, dx, dy, dWidth, dHeight)
// drawImage(image, sx, sy, sWidth, sHeight, dx, dy, dWidth, dHeight)
static DaiValue
Canvas_drawImage(DaiVM* vm, DaiValue receiver, int argc, DaiValue* argv) {
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
    CanvasStruct* canvas = (CanvasStruct*)AS_STRUCT(receiver);
    TextureStruct* obj   = (TextureStruct*)AS_STRUCT(argv[0]);

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
    if (!SDL_RenderTexture(canvas->renderer, texture, &srcrect, &dstrect)) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "SDL could not draw image! SDL_Error: %s", SDL_GetError());
        return OBJ_VAL(err);
    }
    return receiver;
}

// drawImageEx(image, srcrect, dstrect, angle, center, flip)
//     image: TextureStruct
//     srcrect: RectStruct
//     dstrect: RectStruct
//     angle: int|float 角度
//     center: PointStruct
//     flip: int 0: none, 1: horizontal, 2: vertical
static DaiValue
Canvas_drawImageEx(DaiVM* vm, __attribute__((unused)) DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 6) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "canvas.drawImageEx() expected 6 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    CanvasStruct* canvas = (CanvasStruct*)AS_STRUCT(receiver);
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
    center.x               = point_obj->x;
    center.y               = point_obj->y;

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
    if (!SDL_RenderTextureRotated(
            canvas->renderer, texture, &srcrect, &dstrect, angle, &center, flip)) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "SDL could not drawEx image! SDL_Error: %s", SDL_GetError());
        return OBJ_VAL(err);
    }
    return NIL_VAL;
}

// loadImage(path)
static DaiValue
Canvas_loadImage(DaiVM* vm, __attribute__((unused)) DaiValue receiver, int argc, DaiValue* argv) {
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
    CanvasStruct* canvas = (CanvasStruct*)AS_STRUCT(receiver);
    const char* path     = AS_CSTRING(argv[0]);
    SDL_Texture* texture = IMG_LoadTexture(canvas->renderer, path);
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

static DaiObjBuiltinFunction canvas_methods[] = {
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "setFillColor",
        .function = Canvas_setFillColor,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "fillRect",
        .function = Canvas_fillRect,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "run",
        .function = Canvas_run,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "clear",
        .function = Canvas_clear,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "addEventListener",
        .function = Canvas_addEventListener,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "quit",
        .function = Canvas_quit,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "drawImage",
        .function = Canvas_drawImage,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "drawImageEx",
        .function = Canvas_drawImageEx,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "loadImage",
        .function = Canvas_loadImage,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name = NULL,
    },
};
// #endregion

static DaiValue
CanvasStruct_get_property(DaiVM* vm, DaiValue receiver, DaiObjString* name) {
    char* cname       = name->chars;
    CanvasStruct* obj = (CanvasStruct*)AS_STRUCT(receiver);
    if (strcmp(cname, "width") == 0) {
        return INTEGER_VAL(obj->width);
    } else if (strcmp(cname, "height") == 0) {
        return INTEGER_VAL(obj->height);
    } else {
        DaiObjError* err =
            DaiObjError_Newf(vm, "'%s' object has not property '%s'", obj->name, name->chars);
        return OBJ_VAL(err);
    }
}

static char*
CanvasStruct_String(DaiValue value, __attribute__((unused)) DaiPtrArray* visited) {
    char buf[128];
    CanvasStruct* obj = (CanvasStruct*)AS_STRUCT(value);
    int length        = snprintf(buf, sizeof(buf), "<struct %s(%p)>", obj->name, obj);
    return strndup(buf, length);
}

static DaiValue
CanvasStruct_get_method(DaiVM* vm, DaiValue receiver, DaiObjString* name) {
    const char* cname = name->chars;
    for (size_t i = 0; i < sizeof(canvas_methods) / sizeof(canvas_methods[0]); i++) {
        if (canvas_methods[i].name == NULL) {
            break;
        }
        if (strcmp(cname, canvas_methods[i].name) == 0) {
            return OBJ_VAL(&canvas_methods[i]);
        }
    }
    DaiObjError* err =
        DaiObjError_Newf(vm, "'%s' object has not method '%s'", canvas_name, name->chars);
    return OBJ_VAL(err);
}

static struct DaiObjOperation canvas_struct_operation = {
    .get_property_func  = CanvasStruct_get_property,
    .set_property_func  = NULL,
    .subscript_get_func = NULL,
    .subscript_set_func = NULL,
    .string_func        = CanvasStruct_String,
    .equal_func         = dai_default_equal,
    .hash_func          = dai_default_hash,
    .iter_init_func     = NULL,
    .iter_next_func     = NULL,
    .get_method_func    = CanvasStruct_get_method,
};

static void
CanvasStruct_destructor(DaiVM* vm, DaiObjStruct* st) {
    CanvasStruct* canvas = (CanvasStruct*)st;
    if (canvas->renderer != NULL) {
        SDL_DestroyRenderer(canvas->renderer);
    }
    if (canvas->window != NULL) {
        SDL_DestroyWindow(canvas->window);
    }
    canvas->running = false;
}

// #endregion

// #region canvas module functions

// canvas module functions:
//      new(width, height)
//      present()
//      Rect(x, y, width, height)
//      Point(x, y)
//      fillCircle(center, radius, r, g, b, a)

static DaiValue
builtin_canvas_new(DaiVM* vm, __attribute__((unused)) DaiValue receiver, int argc, DaiValue* argv) {
    if (argc != 2) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "canvas.new() expected 2 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (!IS_INTEGER(argv[0]) || !IS_INTEGER(argv[1])) {
        DaiObjError* err = DaiObjError_Newf(
            vm, "canvas.new() expected integer arguments, but got %s", dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
    int width  = AS_INTEGER(argv[0]);
    int height = AS_INTEGER(argv[1]);

    CanvasStruct* canvas = (CanvasStruct*)DaiObjStruct_New(
        vm, canvas_name, &canvas_struct_operation, sizeof(CanvasStruct), CanvasStruct_destructor);
    canvas->width   = width;
    canvas->height  = height;
    canvas->running = false;

    canvas->keydown_callbacks     = DaiObjTuple_New(vm);
    canvas->keyup_callbacks       = DaiObjTuple_New(vm);
    canvas->mousedown_callbacks   = DaiObjTuple_New(vm);
    canvas->mouseup_callbacks     = DaiObjTuple_New(vm);
    canvas->mousemotion_callbacks = DaiObjTuple_New(vm);

    DaiObjStruct_add_ref(vm, (DaiObjStruct*)canvas, OBJ_VAL(canvas->keydown_callbacks));
    DaiObjStruct_add_ref(vm, (DaiObjStruct*)canvas, OBJ_VAL(canvas->keyup_callbacks));
    DaiObjStruct_add_ref(vm, (DaiObjStruct*)canvas, OBJ_VAL(canvas->mousedown_callbacks));
    DaiObjStruct_add_ref(vm, (DaiObjStruct*)canvas, OBJ_VAL(canvas->mouseup_callbacks));
    DaiObjStruct_add_ref(vm, (DaiObjStruct*)canvas, OBJ_VAL(canvas->mousemotion_callbacks));

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

    canvas->window = SDL_CreateWindow("Dai Canvas", width, height, SDL_WINDOW_RESIZABLE);
    if (canvas->window == NULL) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "SDL could not create window! SDL_Error: %s", SDL_GetError());
        return OBJ_VAL(err);
    }

    canvas->renderer = SDL_CreateRenderer(canvas->window, NULL);
    if (canvas->renderer == NULL) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "SDL could not create renderer! SDL_Error: %s", SDL_GetError());
        return OBJ_VAL(err);
    }

    if (!SDL_SetRenderDrawBlendMode(canvas->renderer, SDL_BLENDMODE_BLEND)) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "SDL could not set blend mode! SDL_Error: %s", SDL_GetError());
        return OBJ_VAL(err);
    }

    return OBJ_VAL(canvas);
}

static DaiValue
builtin_canvas_present(DaiVM* vm, __attribute__((unused)) DaiValue receiver, int argc,
                       DaiValue* argv) {
    if (argc != 0) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "canvas.present() expected 0 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    // CHECK_INIT();
    // if (!SDL_RenderPresent(renderer)) {
    //     DaiObjError* err =
    //         DaiObjError_Newf(vm, "SDL could not present window! SDL_Error: %s", SDL_GetError());
    //     return OBJ_VAL(err);
    // }
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
    DaiObjStruct* obj =
        DaiObjStruct_New(vm, rect_struct_name, &rect_struct_operation, sizeof(RectStruct), NULL);
    RectStruct* rect = (RectStruct*)obj;
    rect->x          = AS_INTEGER(argv[0]);
    rect->y          = AS_INTEGER(argv[1]);
    rect->width      = AS_INTEGER(argv[2]);
    rect->height     = AS_INTEGER(argv[3]);
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
        if (!IS_NUMBER(argv[i])) {
            DaiObjError* err = DaiObjError_Newf(
                vm, "canvas.Point() expected number arguments, but got %s", dai_value_ts(argv[i]));
            return OBJ_VAL(err);
        }
    }
    PointStruct* obj = (PointStruct*)DaiObjStruct_New(
        vm, point_struct_name, &point_struct_operation, sizeof(PointStruct), NULL);
    if (IS_INTEGER(argv[0])) {
        obj->x = AS_INTEGER(argv[0]);
    } else {
        obj->x = AS_FLOAT(argv[0]);
    }
    if (IS_INTEGER(argv[1])) {
        obj->y = AS_INTEGER(argv[1]);
    } else {
        obj->y = AS_FLOAT(argv[1]);
    }
    return OBJ_VAL(obj);
}

// code ref: https://gist.github.com/Gumichan01/332c26f6197a432db91cc4327fcabb1c
bool
fill_circle(SDL_Renderer* renderer, float x, float y, float radius) {
    float offsetx, offsety, d;
    int status;


    offsetx = 0;
    offsety = radius;
    d       = radius - 1;
    status  = 0;

    while (offsety >= offsetx) {

        status += SDL_RenderLine(renderer, x - offsety, y + offsetx, x + offsety, y + offsetx);
        status += SDL_RenderLine(renderer, x - offsetx, y + offsety, x + offsetx, y + offsety);
        status += SDL_RenderLine(renderer, x - offsetx, y - offsety, x + offsetx, y - offsety);
        status += SDL_RenderLine(renderer, x - offsety, y - offsetx, x + offsety, y - offsetx);

        if (status < 4) {
            return false;
        }

        if (d >= 2 * offsetx) {
            d -= 2 * offsetx + 1;
            offsetx += 1;
        } else if (d < 2 * (radius - offsety)) {
            d += 2 * offsety - 1;
            offsety -= 1;
        } else {
            d += 2 * (offsety - offsetx - 1);
            offsety -= 1;
            offsetx += 1;
        }
    }

    return true;
}

// fillCircle(center, radius, r, g, b, a)
static DaiValue
builtin_canvas_fillCircle(DaiVM* vm, __attribute__((unused)) DaiValue receiver, int argc,
                          DaiValue* argv) {
    if (argc != 6) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "canvas.fillCircle() expected 6 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    if (!IS_STRUCT(argv[0]) || AS_STRUCT(argv[0])->name != point_struct_name) {
        DaiObjError* err =
            DaiObjError_Newf(vm,
                             "canvas.fillCircle() expected struct %s argument, but got %s",
                             point_struct_name,
                             dai_value_ts(argv[0]));
        return OBJ_VAL(err);
    }
    for (int i = 1; i < 6; i++) {
        if (!IS_INTEGER(argv[i])) {
            DaiObjError* err =
                DaiObjError_Newf(vm,
                                 "canvas.fillCircle() expected integer arguments, but got %s",
                                 dai_value_ts(argv[i]));
            return OBJ_VAL(err);
        }
    }
    // CHECK_INIT();
    // PointStruct* center = (PointStruct*)AS_STRUCT(argv[0]);
    // int radius          = AS_INTEGER(argv[1]);
    // int r               = AS_INTEGER(argv[2]);
    // int g               = AS_INTEGER(argv[3]);
    // int b               = AS_INTEGER(argv[4]);
    // int a               = AS_INTEGER(argv[5]);

    // if (!SDL_SetRenderDrawColor(renderer, r, g, b, a)) {
    //     DaiObjError* err =
    //         DaiObjError_Newf(vm, "SDL could not set draw color! SDL_Error: %s", SDL_GetError());
    //     return OBJ_VAL(err);
    // }
    // fill_circle(renderer, center->x, center->y, radius);
    // return NIL_VAL;

    // // Drawing horizontal lines for each y from top to bottom of the circle
    // for (int y = -radius; y <= radius; y++) {
    //     // At each height y, calculate the width of the circle using the Pythagorean theorem
    //     float horizontalLineLength = (float)sqrt(radius * radius - y * y);

    //     // Draw a line from left to right edge of the circle at height y
    //     bool success = SDL_RenderLine(renderer,
    //                                   center->x - horizontalLineLength,
    //                                   center->y + y,
    //                                   center->x + horizontalLineLength,
    //                                   center->y + y);
    //     if (!success) {
    //         DaiObjError* err =
    //             DaiObjError_Newf(vm, "SDL could not draw circle! SDL_Error: %s", SDL_GetError());
    //         return OBJ_VAL(err);
    //     }
    // }
    return NIL_VAL;
}

// static void
// write_cb(void* closure, void* data, int size) {
// SDL_Texture** texture_p = (SDL_Texture**)closure;
// SDL_IOStream* stream    = SDL_IOFromConstMem(data, size);
// *texture_p              = IMG_LoadTextureTyped_IO(renderer, stream, true, "PNG");
// }

//
static DaiValue
builtin_canvas_smile(DaiVM* vm, __attribute__((unused)) DaiValue receiver, int argc,
                     DaiValue* argv) {
    if (argc != 0) {
        DaiObjError* err =
            DaiObjError_Newf(vm, "canvas.smile() expected 0 arguments, but got %d", argc);
        return OBJ_VAL(err);
    }
    // CHECK_INIT();
    // const int width  = 150;
    // const int height = 150;

    // const float center_x     = width / 2.f;
    // const float center_y     = height / 2.f;
    // const float face_radius  = 70;
    // const float mouth_radius = 50;
    // const float eye_radius   = 10;
    // const float eye_offset_x = 25;
    // const float eye_offset_y = 20;
    // const float eye_x        = center_x - eye_offset_x;
    // const float eye_y        = center_y - eye_offset_y;

    // plutovg_surface_t* surface = plutovg_surface_create(width, height);
    // plutovg_canvas_t* canvas   = plutovg_canvas_create(surface);

    // plutovg_canvas_save(canvas);
    // plutovg_canvas_arc(canvas, center_x, center_y, face_radius, 0, PLUTOVG_TWO_PI, 0);
    // plutovg_canvas_set_rgb(canvas, 1, 1, 0);
    // plutovg_canvas_fill_preserve(canvas);
    // plutovg_canvas_set_rgb(canvas, 0, 0, 0);
    // plutovg_canvas_set_line_width(canvas, 5);
    // plutovg_canvas_stroke(canvas);
    // plutovg_canvas_restore(canvas);

    // plutovg_canvas_save(canvas);
    // plutovg_canvas_arc(canvas, eye_x, eye_y, eye_radius, 0, PLUTOVG_TWO_PI, 0);
    // plutovg_canvas_arc(canvas, center_x + eye_offset_x, eye_y, eye_radius, 0, PLUTOVG_TWO_PI, 0);
    // plutovg_canvas_set_rgb(canvas, 0, 0, 0);
    // plutovg_canvas_fill(canvas);
    // plutovg_canvas_restore(canvas);

    // plutovg_canvas_save(canvas);
    // plutovg_canvas_arc(canvas, center_x, center_y, mouth_radius, 0, PLUTOVG_PI, 0);
    // plutovg_canvas_set_rgb(canvas, 0, 0, 0);
    // plutovg_canvas_set_line_width(canvas, 5);
    // plutovg_canvas_stroke(canvas);
    // plutovg_canvas_restore(canvas);

    // SDL_Texture* texture = NULL;
    // plutovg_surface_write_to_png_stream(surface, write_cb, &texture);
    // if (texture == NULL) {
    //     DaiObjError* err =
    //         DaiObjError_Newf(vm, "SDL could not create texture! SDL_Error: %s", SDL_GetError());
    //     return OBJ_VAL(err);
    // }
    // // Draw the texture to the screen
    // SDL_FRect srcrect = {.x = 0, .y = 0, .w = width, .h = height};
    // SDL_FRect dstrect = {.x = 0, .y = 0, .w = width, .h = height};
    // if (!SDL_RenderTexture(renderer, texture, &srcrect, &dstrect)) {
    //     DaiObjError* err =
    //         DaiObjError_Newf(vm, "SDL could not draw image! SDL_Error: %s", SDL_GetError());
    //     return OBJ_VAL(err);
    // }
    // SDL_DestroyTexture(texture);
    // plutovg_canvas_destroy(canvas);
    // plutovg_surface_destroy(surface);
    return NIL_VAL;
}

static DaiObjBuiltinFunction canvas_funcs[] = {
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "new",
        .function = builtin_canvas_new,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "present",
        .function = builtin_canvas_present,
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
        .name     = "fillCircle",
        .function = builtin_canvas_fillCircle,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name     = "smile",
        .function = builtin_canvas_smile,
    },
    {
        {.type = DaiObjType_builtinFn, .operation = &builtin_function_operation},
        .name = NULL,
    },
};

// #endregion

static DaiSimpleObjectStruct*
keycode_new(DaiVM* vm) {
    DaiSimpleObjectStruct* keycode = DaiSimpleObjectStruct_New(vm, keycode_struct_name, true);
    if (keycode == NULL) {
        fprintf(stderr, "Failed to create keycode struct\n");
        abort();
    }

    for (size_t i = 0; i < sizeof(keycodes) / sizeof(keycodes[0]); i++) {
        DaiObjError* err = DaiSimpleObjectStruct_add(
            vm, keycode, keycodes[i].name, INTEGER_VAL(keycodes[i].value));
        if (err != NULL) {
            fprintf(stderr, "Failed to add keycode %s: %s\n", keycodes[i].name, err->message);
            abort();
        }
    }
    return keycode;
}

static DaiSimpleObjectStruct*
mousebutton_new(DaiVM* vm) {
    DaiSimpleObjectStruct* mousebutton =
        DaiSimpleObjectStruct_New(vm, mousebutton_struct_name, true);
    if (mousebutton == NULL) {
        fprintf(stderr, "Failed to create mousebutton struct\n");
        abort();
    }
    const char* names[] = {"left", "middle", "right"};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        DaiObjError* err = DaiSimpleObjectStruct_add(vm, mousebutton, names[i], INTEGER_VAL(i + 1));
        if (err != NULL) {
            fprintf(stderr, "Failed to add mousebutton %s: %s\n", names[i], err->message);
            abort();
        }
    }
    return mousebutton;
}

static DaiSimpleObjectStruct*
flipmode_new(DaiVM* vm) {
    DaiSimpleObjectStruct* flipmode = DaiSimpleObjectStruct_New(vm, flipmode_struct_name, true);
    if (flipmode == NULL) {
        fprintf(stderr, "Failed to create flipmode struct\n");
        abort();
    }
    const char* names[] = {"none", "horizontal", "vertical"};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        DaiObjError* err = DaiSimpleObjectStruct_add(vm, flipmode, names[i], INTEGER_VAL(i));
        if (err != NULL) {
            fprintf(stderr, "Failed to add flipmode %s: %s\n", names[i], err->message);
            abort();
        }
    }
    return flipmode;
}

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

    DaiSimpleObjectStruct* keycode = keycode_new(vm);
    DaiObjModule_add_global(module, "Keycode", OBJ_VAL(keycode));
    DaiSimpleObjectStruct* mousebutton = mousebutton_new(vm);
    DaiObjModule_add_global(module, "MouseButton", OBJ_VAL(mousebutton));
    DaiSimpleObjectStruct* flipmode = flipmode_new(vm);
    DaiObjModule_add_global(module, "FlipMode", OBJ_VAL(flipmode));
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
    SDL_Quit();
}