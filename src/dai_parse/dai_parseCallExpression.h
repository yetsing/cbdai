#ifndef AB48B11F_9945_4863_A27A_FE167B24E2FF
#define AB48B11F_9945_4863_A27A_FE167B24E2FF

static void
Parser_FreeCallArguments(DaiAstExpression** args, size_t arg_count) {
    for (size_t i = 0; i < arg_count; i++) {
        args[i]->free_fn((DaiAstBase*)args[i], true);
    }
    dai_free(args);
}

// 返回 NULL 表示解析失败
static DaiAstExpression**
Parser_parseCallArguments(Parser* p, size_t* arg_count) {
    size_t arg_size         = 4;
    DaiAstExpression** args = (DaiAstExpression**)dai_malloc(sizeof(DaiAstExpression*) * arg_size);
    *arg_count              = 0;

    // 没有调用参数
    if (Parser_peekTokenIs(p, DaiTokenType_rparen)) {
        Parser_nextToken(p);
        *arg_count = 0;
        return args;
    }
    // 跳过 ( 括号 token
    Parser_nextToken(p);
    DaiAstExpression* arg = Parser_parseExpression(p, Precedence_Lowest);
    if (arg == NULL) {
        Parser_FreeCallArguments(args, *arg_count);
        return NULL;
    }
    args[*arg_count] = arg;
    (*arg_count)++;
    while (Parser_peekTokenIs(p, DaiTokenType_comma)) {
        // 跳过 , 逗号 token
        {
            Parser_nextToken(p);
            if (Parser_peekTokenIs(p, DaiTokenType_rparen)) {
                // 支持参数末尾多余的逗号，例如 (a, b,)b 后面的逗号
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
            Parser_FreeCallArguments(args, *arg_count);
            return NULL;
        }
        args[*arg_count] = arg;
        (*arg_count)++;
    }
    // 结尾的 ) 括号 token
    if (!Parser_expectPeek(p, DaiTokenType_rparen)) {
        Parser_FreeCallArguments(args, *arg_count);
        return NULL;
    }
    return args;
}

static DaiAstExpression*
Parser_parseCallExpression(Parser* p, DaiAstExpression* left) {
    DaiAstCallExpression* call = DaiAstCallExpression_New();
    {
        call->start_line   = left->start_line;
        call->start_column = left->start_column;
    }
    call->function  = left;
    call->arguments = Parser_parseCallArguments(p, &call->arguments_count);
    if (call->arguments == NULL) {
        call->free_fn((DaiAstBase*)call, true);
        return NULL;
    }
    {
        call->end_line   = p->cur_token->end_line;
        call->end_column = p->cur_token->end_column;
    }
    return (DaiAstExpression*)call;
}

#endif /* AB48B11F_9945_4863_A27A_FE167B24E2FF */
