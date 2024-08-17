//
// Created by  on 2024/6/5.
//

#ifndef CBDAI_SRC_DAI_PARSER_DAI_PARSEVARSTATEMENT_H_
#define CBDAI_SRC_DAI_PARSER_DAI_PARSEVARSTATEMENT_H_

static DaiAstVarStatement*
Parser_parseVarStatement(Parser* p) {
    DaiAstVarStatement* stmt = DaiAstVarStatement_New(NULL, NULL);
    stmt->start_line         = p->cur_token->start_line;
    stmt->start_column       = p->cur_token->start_column;

    // 下一个 token 应该是 identifier
    if (!Parser_expectPeek(p, DaiTokenType_ident)) {
        stmt->free_fn((DaiAstBase*)stmt, true);
        return NULL;
    }

    // 创建 identifier 节点
    DaiAstIdentifier* name = DaiAstIdentifier_New(p->cur_token);
    stmt->name             = name;

    // 下一个 token 应该是 assign
    if (!Parser_expectPeek(p, DaiTokenType_assign)) {
        stmt->free_fn((DaiAstBase*)stmt, true);
        return NULL;
    }
    // 跳过 assign token
    Parser_nextToken(p);

    stmt->value = Parser_parseExpression(p, Precedence_Lowest);
    if (stmt->value == NULL) {
        stmt->free_fn((DaiAstBase*)stmt, true);
        return NULL;
    }

    // 语句结束末尾必须是分号
    if (!Parser_expectPeek(p, DaiTokenType_semicolon)) {
        stmt->free_fn((DaiAstBase*)stmt, true);
        return NULL;
    }

    stmt->end_line   = p->cur_token->end_line;
    stmt->end_column = p->cur_token->end_column;
    return stmt;
}

#endif   // CBDAI_SRC_DAI_PARSER_DAI_PARSEVARSTATEMENT_H_
