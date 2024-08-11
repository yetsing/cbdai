#ifndef DB5BBC74_AA02_4C1C_BFE0_FBEAF87FA59C
#define DB5BBC74_AA02_4C1C_BFE0_FBEAF87FA59C

#include "dai_parse/dai_parsefunctionliteral.h"

DaiAstMethodStatement* Parser_parseMethodStatement(Parser* p) {
    DaiAstMethodStatement* func = DaiAstMethodStatement_New();
    {
        func->start_line = p->cur_token->start_line;
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
        func->end_line = p->cur_token->end_line;
        func->end_column = p->cur_token->end_column;
    }
    return func;
}

#endif /* DB5BBC74_AA02_4C1C_BFE0_FBEAF87FA59C */
