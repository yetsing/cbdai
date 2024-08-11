//
// Created by  on 2024/6/5.
//

#ifndef CBDAI_SRC_DAI_PARSER_DAI_PARSEIDENTIFIER_H_
#define CBDAI_SRC_DAI_PARSER_DAI_PARSEIDENTIFIER_H_

// 解析标识符
static DaiAstExpression *Parser_parseIdentifier(Parser *p) {
    daiassert(p->cur_token->type == DaiTokenType_ident, "not an identifier: %s", DaiTokenType_string(p->cur_token->type));
    return (DaiAstExpression *) DaiAstIdentifier_New(p->cur_token);
}

#endif //CBDAI_SRC_DAI_PARSER_DAI_PARSEIDENTIFIER_H_
