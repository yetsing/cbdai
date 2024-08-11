#ifndef B352DB03_C5F9_4152_9D26_D53F9AFE335F
#define B352DB03_C5F9_4152_9D26_D53F9AFE335F

static void Parser_FreeFunctionParameters(DaiAstIdentifier **params,
                                          size_t param_count) {
  for (size_t i = 0; i < param_count; i++) {
    params[i]->free_fn((DaiAstBase *)params[i], true);
  }
  dai_free(params);
}

// 返回 NULL 表示解析失败
static DaiAstIdentifier **Parser_parseFunctionParameters(Parser *p,
                                                         size_t *param_count) {
  size_t param_size = 4;
  DaiAstIdentifier **params =
      dai_malloc(sizeof(DaiAstIdentifier *) * param_size);
  *param_count = 0;

  // 没有参数
  if (Parser_peekTokenIs(p, DaiTokenType_rparen)) {
    Parser_nextToken(p);
    *param_count = 0;
    return params;
  }

  // 第一个参数
  if (!Parser_expectPeek(p, DaiTokenType_ident)) {
    Parser_FreeFunctionParameters(params, *param_count);
    return NULL;
  }
  DaiAstIdentifier *param = DaiAstIdentifier_New(p->cur_token);
  params[*param_count] = param;
  (*param_count)++;
  // 解析后面的参数，格式为 (, ident) ...
  while (Parser_peekTokenIs(p, DaiTokenType_comma)) {
    Parser_nextToken(p);
    if (Parser_peekTokenIs(p, DaiTokenType_rparen)) {
      // 支持参数末尾多余的逗号，例如 (a, b,) ，b 后面的逗号
      break;
    }
    if (!Parser_expectPeek(p, DaiTokenType_ident)) {
      Parser_FreeFunctionParameters(params, *param_count);
      return NULL;
    }
    // 扩容
    {
      if (*param_count == param_size) {
        param_size *= 2;
        params = dai_realloc(params, sizeof(DaiAstIdentifier *) * param_size);
        for (size_t i = *param_count; i < param_size; i++) {
          params[i] = NULL;
        }
      }
    }

    param = DaiAstIdentifier_New(p->cur_token);
    params[*param_count] = param;
    (*param_count)++;
  }
  // 结尾右括号
  if (!Parser_expectPeek(p, DaiTokenType_rparen)) {
    Parser_FreeFunctionParameters(params, *param_count);
    return NULL;
  }
  return params;
}

static DaiAstExpression *Parser_parseFunctionLiteral(Parser *p) {
  DaiAstFunctionLiteral *func = DaiAstFunctionLiteral_New();
  {
    func->start_line = p->cur_token->start_line;
    func->start_column = p->cur_token->start_column;
  }

  // 解析函数参数
  if (!Parser_expectPeek(p, DaiTokenType_lparen)) {
    func->free_fn((DaiAstBase *)func, true);
    return NULL;
  }
  func->parameters = Parser_parseFunctionParameters(p, &func->parameters_count);
  if (func->parameters == NULL) {
    func->free_fn((DaiAstBase *)func, true);
    return NULL;
  }
  // 解析函数体
  if (!Parser_expectPeek(p, DaiTokenType_lbrace)) {
    func->free_fn((DaiAstBase *)func, true);
    return NULL;
  }
  func->body = Parser_parseBlockStatement(p);
  if (func->body == NULL) {
    func->free_fn((DaiAstBase *)func, true);
    return NULL;
  }

  {
    func->end_line = p->cur_token->end_line;
    func->end_column = p->cur_token->end_column;
  }
  return (DaiAstExpression *)func;
}

#endif /* B352DB03_C5F9_4152_9D26_D53F9AFE335F */
