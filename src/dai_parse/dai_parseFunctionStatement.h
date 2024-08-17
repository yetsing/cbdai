#ifndef A853B09F_7EC9_48F2_A4CA_4CCCA21C7592
#define A853B09F_7EC9_48F2_A4CA_4CCCA21C7592

#include "dai_parse/dai_parsefunctionliteral.h"

DaiAstFunctionStatement*
Parser_parseFunctionStatement(Parser* p) {
    DaiAstFunctionStatement* func = DaiAstFunctionStatement_New();
    {
        func->start_line   = p->cur_token->start_line;
        func->start_column = p->cur_token->start_column;
    }
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

#endif /* A853B09F_7EC9_48F2_A4CA_4CCCA21C7592 */
