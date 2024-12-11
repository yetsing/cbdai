#ifndef DAI_PARSEARRAYLITERAL_H
#define DAI_PARSEARRAYLITERAL_H

static void
Parser_FreeExpressionList(DaiAstExpression** list, size_t count) {
    for (size_t i = 0; i < count; i++) {
        list[i]->free_fn((DaiAstBase*)list[i], true);
    }
    dai_free(list);
}


// 返回 NULL 表示解析失败
static DaiAstExpression**
Parser_parseExpressionList(Parser* p, const DaiTokenType end, size_t* arg_count) {
    size_t arg_size         = 4;
    DaiAstExpression** args = (DaiAstExpression**)dai_malloc(sizeof(DaiAstExpression*) * arg_size);
    *arg_count              = 0;

    // 没有调用参数
    if (Parser_peekTokenIs(p, end)) {
        Parser_nextToken(p);
        *arg_count = 0;
        return args;
    }
    // 跳过 ( 括号 token
    Parser_nextToken(p);
    DaiAstExpression* arg = Parser_parseExpression(p, Precedence_Lowest);
    if (arg == NULL) {
        Parser_FreeExpressionList(args, *arg_count);
        return NULL;
    }
    args[*arg_count] = arg;
    (*arg_count)++;
    while (Parser_peekTokenIs(p, DaiTokenType_comma)) {
        // 跳过 , 逗号 token
        {
            Parser_nextToken(p);
            if (Parser_peekTokenIs(p, end)) {
                // 支持参数末尾多余的逗号，例如 (a, b,) b 后面的逗号
                break;
            }
            Parser_nextToken(p);
        }
        {
            if (*arg_count == arg_size) {
                arg_size *= 2;
                args = (DaiAstExpression**)dai_realloc(args, sizeof(DaiAstExpression*) * arg_size);
            }
        }
        arg = Parser_parseExpression(p, Precedence_Lowest);
        if (arg == NULL) {
            Parser_FreeExpressionList(args, *arg_count);
            return NULL;
        }
        args[*arg_count] = arg;
        (*arg_count)++;
    }
    // 结尾的 ) 括号 token
    if (!Parser_expectPeek(p, end)) {
        Parser_FreeExpressionList(args, *arg_count);
        return NULL;
    }
    return args;
}

DaiAstExpression*
Parser_parseArrayLiteral(Parser* p) {
    DaiAstArrayLiteral* array = DaiAstArrayLiteral_New();
    {
        array->start_line   = p->cur_token->start_line;
        array->start_column = p->cur_token->start_column;
    }
    array->elements = Parser_parseExpressionList(p, DaiTokenType_rbracket, &array->length);
    if (array->elements == NULL) {
        array->free_fn((DaiAstBase*)array, true);
        return NULL;
    }
    {
        array->end_line   = p->cur_token->end_line;
        array->end_column = p->cur_token->end_column;
    }
    return (DaiAstExpression*)array;
}

#endif   // DAI_PARSEARRAYLITERAL_H
