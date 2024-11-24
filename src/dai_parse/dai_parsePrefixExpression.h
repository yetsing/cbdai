//
// Created by  on 2024/6/5.
//

#ifndef CBDAI_SRC_DAI_PARSER_DAI_PARSEPREFIXEXPRESSION_H_
#define CBDAI_SRC_DAI_PARSER_DAI_PARSEPREFIXEXPRESSION_H_

// 解析前缀表达式，如 "-1" "!3"
static DaiAstExpression*
Parser_parsePrefixExpression(Parser* p) {
    DaiAstPrefixExpression* prefix =
        (DaiAstPrefixExpression*)DaiAstPrefixExpression_New(p->cur_token, NULL);
    Parser_nextToken(p);
    prefix->right = Parser_parseExpression(p, Precedence_Prefix);
    if (prefix->right == NULL) {
        prefix->free_fn((DaiAstBase*)prefix, true);
        return NULL;
    }
    prefix->end_line   = prefix->right->end_line;
    prefix->end_column = prefix->right->end_column;
    return (DaiAstExpression*)prefix;
}

#endif   // CBDAI_SRC_DAI_PARSER_DAI_PARSEPREFIXEXPRESSION_H_
