#include "dai_canvas.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "SDL3/SDL_keyboard.h"
#include "hashmap.h"
#include <SDL3/SDL.h>

#include "dai_object.h"
#include "dai_value.h"
#include "dai_vm.h"

static SDL_Window* window          = NULL;
static SDL_Renderer* renderer      = NULL;
static const char* module_name     = "canvas";
static bool is_running             = false;
static struct hashmap* keycode_map = NULL;

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
// static EventCallbacks keydown_callbacks  = {
//      .callback_count = 0,
// };
// static EventCallbacks keyup_callbacks = {
//     .callback_count = 0,
// };
// static EventCallbacks mousedown_callbacks = {
//     .callback_count = 0,
// };
// static EventCallbacks mouseup_callbacks = {
//     .callback_count = 0,
// };
// static EventCallbacks mousemotion_callbacks = {
//     .callback_count = 0,
// };

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
        .name = NULL,
    },
};

// #endregion

static DaiObjModule*
create_canvas_module(DaiVM* vm) {

    DaiObjModule* module = DaiObjModule_New(vm, strdup(module_name), strdup("<builtin>"));

    DaiObjModule_add_global(module, "EventKeyDown", INTEGER_VAL(SDL_EVENT_KEY_DOWN));
    DaiObjModule_add_global(module, "EventKeyUp", INTEGER_VAL(SDL_EVENT_KEY_UP));
    DaiObjModule_add_global(module, "EventMouseDown", INTEGER_VAL(SDL_EVENT_MOUSE_BUTTON_DOWN));
    DaiObjModule_add_global(module, "EventMouseUp", INTEGER_VAL(SDL_EVENT_MOUSE_BUTTON_UP));
    DaiObjModule_add_global(module, "EventMouseMotion", INTEGER_VAL(SDL_EVENT_MOUSE_MOTION));

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
    if (keycode_map != NULL) {
        hashmap_free(keycode_map);
        keycode_map = NULL;
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