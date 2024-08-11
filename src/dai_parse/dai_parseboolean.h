#ifndef A3FCAC42_2387_4779_8762_9288F9E88AF5
#define A3FCAC42_2387_4779_8762_9288F9E88AF5

static DaiAstExpression *
Parser_parseBoolean(Parser *p) {
    daiassert(p->cur_token->type == DaiTokenType_true || p->cur_token->type == DaiTokenType_false, "not a boolean: %s", DaiTokenType_string(p->cur_token->type));
    return (DaiAstExpression *) DaiAstBoolean_New(p->cur_token);
}

#endif /* A3FCAC42_2387_4779_8762_9288F9E88AF5 */
