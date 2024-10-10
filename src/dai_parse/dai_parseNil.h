#ifndef SRC_DAI_PARSENIL_H_
#define SRC_DAI_PARSENIL_H_

static DaiAstExpression*
Parser_parseNil(Parser* p) {
    daiassert(p->cur_token->type == DaiTokenType_nil,
              "not a nil: %s",
              DaiTokenType_string(p->cur_token->type));
    return (DaiAstExpression*)DaiAstNil_New(p->cur_token);
}

#endif /* SRC_DAI_PARSENIL_H_ */
