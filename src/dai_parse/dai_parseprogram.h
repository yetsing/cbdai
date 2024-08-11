//
// Created by on 2024/6/5.
//

#ifndef CBDAI_SRC_DAI_PARSER_DAI_PARSEPROGRAM_H_
#define CBDAI_SRC_DAI_PARSER_DAI_PARSEPROGRAM_H_

#include "dai_parse/dai_parsestatement.h"

// 解析程序
static DaiSyntaxError *Parser_parseProgram(Parser *p, DaiAstProgram *program) {

    while (p->cur_token->type != DaiTokenType_eof) {
        DaiAstStatement *stmt = Parser_parseStatement(p);
        if (stmt == NULL) {
            // 语法错误，直接返回
            break;
        }
        DaiAstProgram_append(program, stmt);
        // 一个语句解析完成后， cur_token 是语句的末尾的 token
        // 所以需要读取下一个 token
        Parser_nextToken(p);
    }
    return p->syntax_error;
}

#endif //CBDAI_SRC_DAI_PARSER_DAI_PARSEPROGRAM_H_
