#include <assert.h>

#include "dai_parse.h"
#include "dai_tokenize.h"

#include "dai_parse/dai_parseprogram.h"


DaiSyntaxError*
dai_parse(DaiTokenList* tlist, DaiAstProgram* program) {
    // 创建解析器
    Parser* parser = Parser_New(tlist);
    // 解析 token 列表，构建 ast
    DaiSyntaxError* err = Parser_parseProgram(parser, program);
    // 释放解析器
    Parser_free(parser);
    return err;
}