#ifndef B4AF985A_8110_4219_9110_44C0986BA928
#define B4AF985A_8110_4219_9110_44C0986BA928

static DaiAstExpression*
Parser_parseInfixExpression(Parser* p, DaiAstExpression* left) {
    // 创建中缀表达式节点，此时 cur_token 是操作符
    DaiAstInfixExpression* expr = DaiAstInfixExpression_New(p->cur_token->literal, left);
    // 获取当前操作的优先级
    Precedence precedence = Parser_curPrecedence(p);
    Parser_nextToken(p);
    // 解析右侧表达式
    DaiAstExpression* right = Parser_parseExpression(p, precedence);
    if (right == NULL) {
        expr->free_fn((DaiAstBase*)expr, true);
        return NULL;
    }
    expr->right      = right;
    expr->end_line   = right->end_line;
    expr->end_column = right->end_column;
    return (DaiAstExpression*)expr;
}

#endif /* B4AF985A_8110_4219_9110_44C0986BA928 */
