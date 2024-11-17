#ifndef B6AEA5F3_A8A5_48BB_B2C0_DED5180BBDD1
#define B6AEA5F3_A8A5_48BB_B2C0_DED5180BBDD1

#include "dai_parse/dai_parseFunctionLiteral.h"

DaiAstClassMethodStatement*
Parser_parseClassMethodStatement(Parser* p) {
    DaiAstClassMethodStatement* func = DaiAstClassMethodStatement_New();
    {
        func->start_line   = p->cur_token->start_line;
        func->start_column = p->cur_token->start_column;
    }
    // 跳过 class 关键字，现在当前 token 是 var
    Parser_nextToken(p);


    if (!Parser_expectPeek(p, DaiTokenType_ident)) {
        func->free_fn((DaiAstBase*)func, true);
        return NULL;
    }
    dai_move(p->cur_token->literal, func->name);

    // 解析函数参数
    if (!Parser_expectPeek(p, DaiTokenType_lparen)) {
        func->free_fn((DaiAstBase*)func, true);
        return NULL;
    }
    func->parameters = Parser_parseFunctionParameters(p, &func->parameters_count);
    if (func->parameters == NULL) {
        func->free_fn((DaiAstBase*)func, true);
        return NULL;
    }
    // 解析函数体
    if (!Parser_expectPeek(p, DaiTokenType_lbrace)) {
        func->free_fn((DaiAstBase*)func, true);
        return NULL;
    }
    func->body = Parser_parseBlockStatement(p);
    if (func->body == NULL) {
        func->free_fn((DaiAstBase*)func, true);
        return NULL;
    }
    if (!Parser_expectPeek(p, DaiTokenType_semicolon)) {
        func->free_fn((DaiAstBase*)func, true);
        return NULL;
    }

    {
        func->end_line   = p->cur_token->end_line;
        func->end_column = p->cur_token->end_column;
    }
    return func;
}

#endif /* B6AEA5F3_A8A5_48BB_B2C0_DED5180BBDD1 */
