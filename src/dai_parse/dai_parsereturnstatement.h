//
// Created by  on 2024/6/5.
//

#ifndef CBDAI_SRC_DAI_PARSER_DAI_ASTRETURNSTATEMENT_H_
#define CBDAI_SRC_DAI_PARSER_DAI_ASTRETURNSTATEMENT_H_

static DaiAstReturnStatement *Parser_parseReturnStatement(Parser *p) {
    DaiAstReturnStatement *stmt = DaiAstReturnStatement_New(NULL);
    stmt->start_line = p->cur_token->start_line;
    stmt->start_column = p->cur_token->start_column;

    // 跳过 return 关键字 token
    Parser_nextToken(p);
    if (Parser_curTokenIs(p, DaiTokenType_semicolon)) {
        // "return;" 返回空
        stmt->end_line = p->cur_token->end_line;
        stmt->end_column = p->cur_token->end_column;
        return stmt;
    }
    // 解析返回值表达式
    stmt->return_value = Parser_parseExpression(p, Precedence_Lowest);
    if (stmt->return_value == NULL) {
        stmt->free_fn((DaiAstBase *)stmt, true);
        return NULL;
    }
    // 语句结尾必须是分号
    if (!Parser_expectPeek(p, DaiTokenType_semicolon)) {
        stmt->free_fn((DaiAstBase *)stmt, true);
        return NULL;
    }

    stmt->end_line = p->cur_token->end_line;
    stmt->end_column = p->cur_token->end_column;
    return stmt;
}

#endif //CBDAI_SRC_DAI_PARSER_DAI_ASTRETURNSTATEMENT_H_
