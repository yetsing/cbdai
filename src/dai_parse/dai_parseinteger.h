//
// Created by  on 2024/6/5.
//

#ifndef CBDAI_SRC_DAI_PARSER_DAI_PARSEINTEGER_H_
#define CBDAI_SRC_DAI_PARSER_DAI_PARSEINTEGER_H_

#include "dai_parseint.h"

// 解析整数字面量
static DaiAstExpression*
Parser_parseInteger(Parser* p) {
    daiassert(p->cur_token->type == DaiTokenType_int,
              "not an integer: %s",
              DaiTokenType_string(p->cur_token->type));
    // 解析字符串为整数
    int base      = 10;
    char* literal = p->cur_token->literal;
    if (strlen(literal) >= 3 && literal[0] == '0') {
        switch (literal[1]) {
            case 'x':
            case 'X': base = 16; break;
            case 'o':
            case 'O': base = 8; break;
            case 'b':
            case 'B': base = 2; break;
        }
    }
    char* error = NULL;
    int64_t n   = dai_parseint(literal, base, &error);
    if (error != NULL) {
        char buf[256];
        snprintf(buf, sizeof(buf), "%s \"%s\"", error, p->cur_token->literal);
        int line   = p->cur_token->start_line;
        int column = p->cur_token->start_column;
        if (p->cur_token->type == DaiTokenType_eof) {
            line   = p->prev_token->end_line;
            column = p->prev_token->end_column;
        }
        p->syntax_error = DaiSyntaxError_New(buf, line, column);
        return NULL;
    }
    // 创建整数节点
    DaiAstIntegerLiteral* num = DaiAstIntegerLiteral_New(p->cur_token);
    num->value                = n;
    return (DaiAstExpression*)num;
}

#endif   // CBDAI_SRC_DAI_PARSER_DAI_PARSEINTEGER_H_
