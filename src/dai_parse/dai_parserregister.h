//
// Created by on 2024/6/5.
//
#ifndef CBAI_SRC_DAI_PARSER_DAI_PARSERREGISTER_H_
#define CBAI_SRC_DAI_PARSER_DAI_PARSERREGISTER_H_

static void
Parser_register(Parser *p) {
    // 初始化
    for (int i = 0; i < DaiTokenType_end; i++) {
        p->prefix_parse_fns[i] = NULL;
        p->infix_parse_fns[i] = NULL;
    }
    // 注册前缀表达式解析函数
    {
        Parser_registerPrefix(p, DaiTokenType_ident, Parser_parseIdentifier);
        Parser_registerPrefix(p, DaiTokenType_int, Parser_parseInteger);
        Parser_registerPrefix(p, DaiTokenType_bang, Parser_parsePrefixExpression);
        Parser_registerPrefix(p, DaiTokenType_minus, Parser_parsePrefixExpression);
        Parser_registerPrefix(p, DaiTokenType_true, Parser_parseBoolean);
        Parser_registerPrefix(p, DaiTokenType_false, Parser_parseBoolean);
        Parser_registerPrefix(p, DaiTokenType_lparen, Parser_parseGroupedExpression);
        Parser_registerPrefix(p, DaiTokenType_function, Parser_parseFunctionLiteral);
        Parser_registerPrefix(p, DaiTokenType_str, Parser_parseStringLiteral);
        Parser_registerPrefix(p, DaiTokenType_self, Parser_parseSelfExpression);
        Parser_registerPrefix(p, DaiTokenType_super, Parser_parseSuperExpression);
    }

    // 注册中缀表达式解析函数
    {
        Parser_registerInfix(p, DaiTokenType_plus, Parser_parseInfixExpression);
        Parser_registerInfix(p, DaiTokenType_minus, Parser_parseInfixExpression);
        Parser_registerInfix(p, DaiTokenType_slash, Parser_parseInfixExpression);
        Parser_registerInfix(p, DaiTokenType_asterisk, Parser_parseInfixExpression);
        Parser_registerInfix(p, DaiTokenType_eq, Parser_parseInfixExpression);
        Parser_registerInfix(p, DaiTokenType_not_eq, Parser_parseInfixExpression);
        Parser_registerInfix(p, DaiTokenType_lt, Parser_parseInfixExpression);
        Parser_registerInfix(p, DaiTokenType_gt, Parser_parseInfixExpression);
        Parser_registerInfix(p, DaiTokenType_lparen, Parser_parseCallExpression);
        Parser_registerInfix(p, DaiTokenType_dot, Parser_parseDotExpression);
    }
}
#endif
