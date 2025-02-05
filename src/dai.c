#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dai.h"
#include "dai_compile.h"
#include "dai_object.h"
#include "dai_parse.h"
#include "dai_tokenize.h"
#include "dai_utils.h"
#include "dai_value.h"
#include "dai_vm.h"

typedef struct Dai {
    DaiVM vm;
    bool loaded;
} Dai;

Dai*
dai_new() {
    Dai* dai = malloc(sizeof(Dai));
    if (dai == NULL) {
        perror("malloc error");
        abort();
    }
    DaiVM_init(&dai->vm);
    dai->loaded = false;
    return dai;
}

void
dai_free(Dai* dai) {
    DaiVM_reset(&dai->vm);
    free(dai);
}

void
dai_load_file(Dai* dai, const char* filename) {
    if (dai->loaded) {
        fprintf(stderr, "dai_load_file can only be called once.\n");
        abort();
    }
    dai->loaded = true;
    // convert to absolute path
    char* filepath = realpath(filename, NULL);
    if (filepath == NULL) {
        perror("realpath error");
        abort();
    }
    char* text = dai_string_from_file(filepath);
    if (text == NULL) {
        perror("open file error");
        abort();
    }
    DaiError* err = NULL;
    DaiTokenList tlist;
    DaiTokenList_init(&tlist);
    DaiAstProgram program;
    DaiAstProgram_init(&program);

    err = dai_tokenize_string(text, &tlist);
    if (err != NULL) {
        DaiSyntaxError_setFilename(err, filename);
        DaiSyntaxError_pprint(err, text);
        goto end;
    }
    err = dai_parse(&tlist, &program);
    if (err != NULL) {
        DaiSyntaxError_setFilename(err, filename);
        DaiSyntaxError_pprint(err, text);
        goto end;
    }
    DaiObjFunction* function = DaiObjFunction_New(&dai->vm, "<main>", filepath);
    err                      = dai_compile(&program, function, &dai->vm);
    if (err != NULL) {
        DaiCompileError_pprint(err, text);
        goto end;
    }
    DaiObjError* runtime_err = DaiVM_run(&dai->vm, function);
    if (runtime_err != NULL) {
        DaiVM_printError(&dai->vm, runtime_err);
        goto end;
    }
end:
    free(filepath);
    free(text);
    if (err != NULL) {
        DaiError_free(err);
    }
    DaiTokenList_reset(&tlist);
    DaiAstProgram_reset(&program);
}

int64_t
dai_get_int(Dai* dai, const char* name) {
    DaiValue value;
    if (!DaiVM_getGlobal(&dai->vm, name, &value)) {
        fprintf(stderr, "dai_get_int: variable '%s' not found.\n", name);
        abort();
    }
    if (!IS_INTEGER(value)) {
        fprintf(stderr,
                "dai_get_int: variable '%s' expected int, but got %s.\n",
                name,
                dai_value_ts(value));
        abort();
    }
    return AS_INTEGER(value);
}

void
dai_set_int(Dai* dai, const char* name, int64_t value) {
    if (!DaiVM_setGlobal(&dai->vm, name, INTEGER_VAL(value))) {
        fprintf(stderr, "dai_set_int: variable '%s' not found.\n", name);
        abort();
    }
}

double
dai_get_float(Dai* dai, const char* name) {
    DaiValue value;
    if (!DaiVM_getGlobal(&dai->vm, name, &value)) {
        fprintf(stderr, "dai_get_float: variable '%s' not found.\n", name);
        abort();
    }
    if (!IS_FLOAT(value)) {
        fprintf(stderr,
                "dai_get_float: variable '%s' expected float, but got %s.\n",
                name,
                dai_value_ts(value));
        abort();
    }
    return AS_FLOAT(value);
}

void
dai_set_float(Dai* dai, const char* name, double value) {
    if (!DaiVM_setGlobal(&dai->vm, name, FLOAT_VAL(value))) {
        fprintf(stderr, "dai_set_float: variable '%s' not found.\n", name);
        abort();
    }
}

const char*
dai_get_string(Dai* dai, const char* name) {
    DaiValue value;
    if (!DaiVM_getGlobal(&dai->vm, name, &value)) {
        fprintf(stderr, "dai_get_string: variable '%s' not found.\n", name);
        abort();
    }
    if (!IS_STRING(value)) {
        fprintf(stderr,
                "dai_get_string: variable '%s' expected string, but got %s.\n",
                name,
                dai_value_ts(value));
        abort();
    }
    return AS_CSTRING(value);
}

void
dai_set_string(Dai* dai, const char* name, const char* value) {
    DaiValue v = OBJ_VAL(dai_copy_string(&dai->vm, value, strlen(value)));
    if (!DaiVM_setGlobal(&dai->vm, name, v)) {
        fprintf(stderr, "dai_set_string: variable '%s' not found.\n", name);
        abort();
    }
}
