#include <assert.h>
#include <stdio.h>

#include "dai_malloc.h"
#include "dai_parse.h"
#include "dai_tokenize.h"

static char* string_from_file(const char* filename) {
    FILE* fp = fopen(filename, "r");
    assert(fp != NULL);
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char* buf = (char*)dai_malloc(size + 1);
    fread(buf, 1, size, fp);
    fclose(fp);
    buf[size] = '\0';
    return buf;
}

static void parse_helper(const char* input, DaiAstProgram* program) {
    DaiTokenList* tlist = DaiTokenList_New();
    DaiSyntaxError* err = dai_tokenize_string(input, tlist);
    if (err != NULL) {
        DaiSyntaxError_setFilename(err, "<stdin>");
        DaiSyntaxError_pprint(err, input);
    }
    assert(err == NULL);
    err = dai_parse(tlist, program);
    if (err != NULL) {
        DaiSyntaxError_setFilename(err, "<stdin>");
        DaiSyntaxError_pprint(err, input);
    }
    assert(err == NULL);
    DaiTokenList_free(tlist);
}

int
main(int argc, const char* argv[]) {
    const char *filename = argv[1];
    char* input = string_from_file(filename);
    DaiAstProgram prog;
    DaiAstProgram_init(&prog);
    DaiAstProgram* program = &prog;
    parse_helper(input, program);

    {
        char* s = program->string_fn((DaiAstBase*)program, true);
        printf("%s\n", s);
        free(s);
    }
    free(input);
    return 0;
}
