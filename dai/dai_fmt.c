/*
    dai formatter
*/
#include "dai_fmt.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "dai_assert.h"
#include "dai_ast.h"
#include "dai_ast/dai_astMapLiteral.h"
#include "dai_ast/dai_astSelfExpression.h"
#include "dai_ast/dai_astclassstatement.h"
#include "dai_ast/dai_astexpression.h"
#include "dai_ast/dai_astinfixexpression.h"
#include "dai_ast/dai_astprefixexpression.h"
#include "dai_ast/dai_astprogram.h"
#include "dai_ast/dai_aststatement.h"
#include "dai_ast/dai_asttype.h"
#include "dai_common.h"
#include "dai_stringbuffer.h"
#include "dai_tokenize.h"


typedef struct {
    DaiStringBuffer* sb;
    size_t indent_level;
    size_t indent_size;
    char indent[16];   // 缩进字符串，最多 16 个空格

    // program 和 tlist 仅为引用
    const DaiAstProgram* program;   // 待格式化的 AST
    const DaiTokenList* tlist;      // token 列表
    DaiToken* last_token;           // 已经输出的最后一个 token

} Formatter;

static void
Formatter_init(Formatter* formatter, const DaiAstProgram* program) {
    formatter->sb           = DaiStringBuffer_New();
    formatter->indent_level = 0;
    formatter->indent_size  = 4;
    assert(formatter->indent_size < sizeof(formatter->indent));
    for (size_t i = 0; i < formatter->indent_size; i++) {
        formatter->indent[i] = ' ';
    }
    formatter->indent[formatter->indent_size] = '\0';

    formatter->program    = program;
    formatter->tlist      = &program->tlist;
    formatter->last_token = NULL;
}

static void
Formatter_reset(Formatter* formatter) {
    if (formatter->sb != NULL) {
        DaiStringBuffer_free(formatter->sb);
    }
    formatter->indent_level = 0;
    formatter->indent_size  = 4;
    formatter->program      = NULL;
}

static char*
Formatter_get_string(Formatter* formatter) {
    size_t length = 0;
    char* s       = DaiStringBuffer_getAndFree(formatter->sb, &length);
    formatter->sb = NULL;
    return s;
}

static void
Formatter_indent(Formatter* formatter) {
    formatter->indent_level++;
}

static void
Formatter_dedent(Formatter* formatter) {
    daiassert(formatter->indent_level > 0, "indent level is 0");
    formatter->indent_level--;
}

// 如果代码的最后一个字符不是换行符，则添加一个换行符
static void
Formatter_append_endline(Formatter* formatter) {
    if (DaiStringBuffer_last(formatter->sb) != '\n') {
        // 如果最后一个字符不是换行符，添加换行符
        DaiStringBuffer_writec(formatter->sb, '\n');
    }
}

static void
Formatter_append_space(Formatter* formatter) {
    if (DaiStringBuffer_last(formatter->sb) != ' ' && DaiStringBuffer_last(formatter->sb) != '\n') {
        // 如果最后一个字符不是空格，添加空格
        DaiStringBuffer_writec(formatter->sb, ' ');
    }
}

static void
Formatter_printn(Formatter* formatter, const char* s, size_t n) {
    if (n == 0) {
        return;
    }
    if (DaiStringBuffer_last(formatter->sb) == '\n' && formatter->indent_level > 0) {
        // 写入换行符后，添加缩进
        // 这里的缩进是为了让下一行的代码缩进
        for (size_t i = 0; i < formatter->indent_level; i++) {
            DaiStringBuffer_writen(formatter->sb, formatter->indent, formatter->indent_size);
        }
    }
    DaiStringBuffer_writen(formatter->sb, s, n);
}

static void
Formatter_print(Formatter* formatter, const char* s) {
    Formatter_printn(formatter, s, strlen(s));
}

static void
Formatter_print_newline(Formatter* formatter, int n) {
#define MAX_NEWLINE_COUNT 10
    if (n == 0) {
        return;
    }
    daiassert(n >= 0 && n < MAX_NEWLINE_COUNT, "newline count is invalid");
    char buf[MAX_NEWLINE_COUNT + 1];
    memset(buf, '\n', n);
    buf[n] = '\0';
    Formatter_printn(formatter, buf, n);
#undef MAX_NEWLINE_COUNT
}

static void
Formatter_print_token(Formatter* formatter, DaiToken* token) {
    formatter->last_token = token;
    if (token->type == DaiTokenType_eof) {
        return;
    }
    Formatter_printn(formatter, token->s, token->length);
    if (token->type == DaiTokenType_comment) {
        // 注释后面添加换行符
        Formatter_print_newline(formatter, 1);
    }
}


// 处理 last_token 之后的注释
static void
Formatter_print_comments(Formatter* formatter) {
    DaiToken* prev_token = formatter->last_token;
    DaiToken* token      = DaiTokenList_get(formatter->tlist, prev_token->index + 1);
    if (token->type != DaiTokenType_comment) {
        // 如果下一个 token 不是注释，直接返回
        return;
    }
    if (token->start_line == prev_token->start_line) {
        // 行尾注释，前面增加空格
        Formatter_print(formatter, "  ");
    } else {
        // 前一个 token 不是注释，需要另起一行
        Formatter_print_newline(formatter, 1);
    }
    while (token->type == DaiTokenType_comment) {
        if (token->start_line > prev_token->start_line + 1) {
            // 中间有空行
            Formatter_print_newline(formatter, 1);
        }
        Formatter_print_token(formatter, token);
        prev_token = token;
        token      = DaiTokenList_get(formatter->tlist, token->index + 1);
    }
}

static void
Formatter_print_token_with_comment(Formatter* formatter, DaiToken* token) {
    Formatter_print_token(formatter, token);
    Formatter_print_comments(formatter);
}

static void
Formatter_print_token1_with_comment(Formatter* formatter, size_t index) {
    DaiToken* token = DaiTokenList_get(formatter->tlist, index);
    Formatter_print_token_with_comment(formatter, token);
}

static void
Formatter_print_next_token(Formatter* formatter) {
    DaiToken* token = DaiTokenList_get(formatter->tlist, formatter->last_token->index + 1);
    Formatter_print_token(formatter, token);
}

static void
Formatter_print_next_token_with_comment(Formatter* formatter) {
    Formatter_print_next_token(formatter);
    Formatter_print_comments(formatter);
}

static bool
Formatter_next_token_is(Formatter* formatter, DaiTokenType type) {
    if (formatter->last_token == NULL) {
        return false;
    }
    DaiToken* token = DaiTokenList_get(formatter->tlist, formatter->last_token->index + 1);
    return token->type == type;
}

static void
Formatter_skip_end_semicolon(Formatter* formatter, DaiAstStatement* stmt) {
    DaiToken* token = DaiTokenList_get(formatter->tlist, stmt->end_token->index - 1);
    if (token->type == DaiTokenType_semicolon) {
        // 输出分号之前的注释
        Formatter_print_comments(formatter);
        // 省略分号
        formatter->last_token = token;
    }
}

// static void
// Formatter_print_token_range(Formatter* formatter, size_t start, size_t end) {
//     for (size_t i = start; i < end; i++) {
//         DaiToken* token = DaiTokenList_get(formatter->tlist, i);
//         if (token->type == DaiTokenType_comment &&
//             token->start_line == formatter->last_token->start_line) {
//             // 行尾注释，前面增加空格
//             Formatter_print(formatter, "  ");
//         }
//         Formatter_print_token(formatter, token);
//     }
// }

static void
Formatter_print_statement(Formatter* formatter, DaiAstStatement* stmt);
static void
Formatter_print_block(Formatter* formatter, DaiAstBlockStatement* stmt);
static void
Formatter_print_expression(Formatter* formatter, DaiAstExpression* expr);

static void
Formatter_print_expression(Formatter* formatter, DaiAstExpression* expr) {
    switch (expr->type) {
        case DaiAstType_IntegerLiteral:
        case DaiAstType_FloatLiteral:
        case DaiAstType_Boolean:
        case DaiAstType_Nil:
        case DaiAstType_Identifier:
        case DaiAstType_StringLiteral: {
            Formatter_print_token_with_comment(formatter, expr->start_token);
            break;
        }
        case DaiAstType_PrefixExpression: {
            DaiAstPrefixExpression* prefix = (DaiAstPrefixExpression*)expr;
            if (prefix->lparen) {
                Formatter_print_token_with_comment(formatter, prefix->lparen);
            }
            Formatter_print_token_with_comment(formatter, prefix->start_token);
            Formatter_print_expression(formatter, prefix->right);
            if (prefix->rparen) {
                Formatter_print_token_with_comment(formatter, prefix->rparen);
            }
            break;
        }
        case DaiAstType_InfixExpression: {
            DaiAstInfixExpression* infix = (DaiAstInfixExpression*)expr;
            if (infix->lparen) {
                Formatter_print_token_with_comment(formatter, infix->lparen);
            }
            Formatter_print_expression(formatter, infix->left);
            Formatter_append_space(formatter);
            Formatter_print_next_token(formatter);
            Formatter_append_space(formatter);
            Formatter_print_comments(formatter);
            Formatter_print_expression(formatter, infix->right);
            if (infix->rparen) {
                Formatter_print_token_with_comment(formatter, infix->rparen);
            }
            break;
        }
        case DaiAstType_FunctionLiteral: {
            DaiAstFunctionLiteral* func = (DaiAstFunctionLiteral*)expr;
            // fn
            Formatter_print_token_with_comment(formatter, func->start_token);
            // (
            Formatter_print_next_token_with_comment(formatter);
            if (func->parameters_count > 0) {
                Formatter_indent(formatter);
                Formatter_append_endline(formatter);
                // 参数列表
                size_t defaults_count = DaiArray_length(func->defaults);
                for (size_t i = 0; i < func->parameters_count; i++) {
                    Formatter_print_expression(formatter, (DaiAstExpression*)func->parameters[i]);
                    if (i >= func->parameters_count - defaults_count) {
                        // =
                        Formatter_print_next_token_with_comment(formatter);
                        // 如果参数有默认值，则输出默认值
                        DaiAstExpression* default_expr = *(DaiAstExpression**)DaiArray_get(
                            func->defaults, i - (func->parameters_count - defaults_count));
                        // 默认值
                        Formatter_print_expression(formatter, default_expr);
                    }
                    // ,
                    if (Formatter_next_token_is(formatter, DaiTokenType_comma)) {
                        Formatter_print_next_token_with_comment(formatter);
                    } else {
                        Formatter_print(formatter, ",\n");
                    }
                    Formatter_append_endline(formatter);
                }
                Formatter_dedent(formatter);
            }
            // )
            Formatter_print_next_token_with_comment(formatter);
            Formatter_append_space(formatter);
            // body
            Formatter_print_block(formatter, func->body);
            // 输出 } 后面的注释
            Formatter_print_comments(formatter);
            break;
        }
        case DaiAstType_ArrayLiteral: {
            DaiAstArrayLiteral* array = (DaiAstArrayLiteral*)expr;
            // [
            Formatter_print_token_with_comment(formatter, array->start_token);
            if (array->length > 0) {
                Formatter_indent(formatter);
                Formatter_append_endline(formatter);
                for (size_t i = 0; i < array->length; i++) {
                    Formatter_print_expression(formatter, array->elements[i]);
                    if (Formatter_next_token_is(formatter, DaiTokenType_comma)) {
                        // ,
                        Formatter_print_next_token_with_comment(formatter);
                        Formatter_append_endline(formatter);
                    } else {
                        Formatter_print(formatter, ",\n");
                    }
                }
                Formatter_dedent(formatter);
            }
            // ]
            Formatter_print_token1_with_comment(formatter, array->end_token->index - 1);
            break;
        }
        case DaiAstType_CallExpression: {
            DaiAstCallExpression* call = (DaiAstCallExpression*)expr;
            Formatter_print_expression(formatter, call->function);
            // (
            Formatter_print_next_token_with_comment(formatter);
            if (call->arguments_count > 0) {
                Formatter_indent(formatter);
                Formatter_append_endline(formatter);
                for (size_t i = 0; i < call->arguments_count; i++) {
                    Formatter_print_expression(formatter, call->arguments[i]);
                    // ,
                    if (Formatter_next_token_is(formatter, DaiTokenType_comma)) {
                        Formatter_print_next_token_with_comment(formatter);
                        Formatter_append_endline(formatter);
                    } else {
                        Formatter_print(formatter, ",\n");
                    }
                }
                Formatter_dedent(formatter);
            }
            // )
            Formatter_print_token1_with_comment(formatter, call->end_token->index - 1);
            break;
        }
        case DaiAstType_DotExpression: {
            DaiAstDotExpression* dot = (DaiAstDotExpression*)expr;
            Formatter_print_expression(formatter, dot->left);
            // .
            Formatter_print_next_token_with_comment(formatter);
            // identifier
            Formatter_print_expression(formatter, (DaiAstExpression*)dot->name);
            break;
        }
        case DaiAstType_SelfExpression: {
            DaiAstSelfExpression* self = (DaiAstSelfExpression*)expr;
            // self
            Formatter_print_token_with_comment(formatter, self->start_token);
            // .
            Formatter_print_next_token_with_comment(formatter);
            // identifier
            Formatter_print_expression(formatter, (DaiAstExpression*)self->name);
            break;
        }
        case DaiAstType_ClassExpression: {
            DaiAstClassExpression* class = (DaiAstClassExpression*)expr;
            // class
            Formatter_print_token_with_comment(formatter, class->start_token);
            // .
            Formatter_print_next_token_with_comment(formatter);
            // identifier
            Formatter_print_expression(formatter, (DaiAstExpression*)class->name);
            break;
        }
        case DaiAstType_SuperExpression: {
            DaiAstSuperExpression* super = (DaiAstSuperExpression*)expr;
            // super
            Formatter_print_token_with_comment(formatter, super->start_token);
            // .
            Formatter_print_next_token_with_comment(formatter);
            // identifier
            Formatter_print_expression(formatter, (DaiAstExpression*)super->name);
            break;
        }
        case DaiAstType_SubscriptExpression: {
            DaiAstSubscriptExpression* subscript = (DaiAstSubscriptExpression*)expr;
            Formatter_print_expression(formatter, subscript->left);
            // [
            Formatter_print_next_token_with_comment(formatter);
            // index
            Formatter_print_expression(formatter, subscript->right);
            // ]
            Formatter_print_token1_with_comment(formatter, subscript->end_token->index - 1);
            break;
        }
        case DaiAstType_MapLiteral: {
            DaiAstMapLiteral* map = (DaiAstMapLiteral*)expr;
            // {
            Formatter_print_token_with_comment(formatter, map->start_token);
            if (map->length > 0) {
                Formatter_indent(formatter);
                Formatter_append_endline(formatter);
                for (size_t i = 0; i < map->length; i++) {
                    DaiAstMapLiteralPair* pair = &map->pairs[i];
                    // key
                    Formatter_print_expression(formatter, pair->key);
                    // :
                    Formatter_print_next_token_with_comment(formatter);
                    Formatter_append_space(formatter);
                    // value
                    Formatter_print_expression(formatter, pair->value);
                    // ,
                    if (Formatter_next_token_is(formatter, DaiTokenType_comma)) {
                        Formatter_print_next_token_with_comment(formatter);
                        Formatter_append_endline(formatter);
                    } else {
                        Formatter_print(formatter, ",\n");
                    }
                }
                Formatter_dedent(formatter);
            }
            // }
            Formatter_print_token1_with_comment(formatter, map->end_token->index - 1);
            break;
        }
        default: unreachable();
    }
}

static void
Formatter_print_block(Formatter* formatter, DaiAstBlockStatement* block_stmt) {
    Formatter_print_token(formatter, block_stmt->start_token);
    Formatter_indent(formatter);
    Formatter_print_comments(formatter);
    Formatter_append_endline(formatter);
    for (size_t i = 0; i < block_stmt->length; i++) {
        Formatter_print_statement(formatter, block_stmt->statements[i]);
    }
    Formatter_dedent(formatter);
    Formatter_print_next_token(formatter);
}

/*
处理语句前的空行和注释

语句前的注释可以分为两种：
    一种紧贴着语句，与语句之间没有多余空行；(leading comment)
    另一种与语句之间有多余空行
例如：
    语句1

    // 注释1，与上下语句之间都有空行

    // 注释2，与语句之间没有空行
    语句2

注释1 和 2 都是语句前的注释
*/
static void
Formatter_print_statement_leading(Formatter* formatter, DaiAstStatement* stmt) {
    static bool first_statement = true;
    if (formatter->last_token && formatter->last_token->index + 1 == stmt->start_token->index) {
        // 如果当前语句的开始 token 紧跟在上一个 token 后面，说明中间没有注释
        switch (stmt->type) {
            case DaiAstType_FunctionStatement:
            case DaiAstType_ClassStatement:
                // 对于函数声明和类声明，统一添加两个空行，与上一条语句分隔开
                Formatter_print_newline(formatter, 2);
                break;
            default: {
                // 其他语句，看原本是否有空行。如果有，则添加一个空行；否则不添加
                DaiToken* token =
                    DaiTokenList_get(formatter->tlist, formatter->last_token->index + 1);
                if (token->start_line > formatter->last_token->start_line + 1) {
                    Formatter_print_newline(formatter, 1);
                }
                break;
            };
        }
        return;
    }

    if (!first_statement) {
        // 找到紧贴语句前的最开始的注释
        // 这里的注释是指与语句之间没有空行的注释
        size_t leading_comment_index = stmt->start_token->index;
        DaiToken* next_token         = stmt->start_token;
        for (size_t i = stmt->start_token->index - 1; i > formatter->last_token->index; i--) {
            DaiToken* token = DaiTokenList_get(formatter->tlist, i);
            if (token->type != DaiTokenType_comment) {
                // 如果不是注释，说明已经到达语句的开始 token
                break;
            }
            if (token->start_line + 1 != next_token->start_line) {
                // 找到紧贴语句注释的开头
                break;
            }
            next_token            = token;
            leading_comment_index = i;
        }
        // 输出没有紧贴语句的注释
        DaiToken* prev_token = formatter->last_token;
        for (size_t i = formatter->last_token->index + 1; i < leading_comment_index; i++) {
            DaiToken* token = DaiTokenList_get(formatter->tlist, i);
            if (token->start_line > prev_token->start_line + 1) {
                // 如果当前 token 的行号大于上一个 token 的行号，说明中间有空行
                Formatter_print_newline(formatter, 1);
            }
            prev_token = token;
            Formatter_print_token(formatter, token);
        }
        // 输出分隔的换行符
        switch (stmt->type) {
            case DaiAstType_FunctionStatement:
            case DaiAstType_ClassStatement:
                // 对于函数声明和类声明，统一添加两个空行，与上一条语句分隔开
                Formatter_print_newline(formatter, 2);
                break;
            default: {
                // 其他语句，看原本是否有空行。如果有，则添加一个空行；否则不添加
                DaiToken* token =
                    DaiTokenList_get(formatter->tlist, formatter->last_token->index + 1);
                if (token->start_line > formatter->last_token->start_line + 1) {
                    Formatter_print_newline(formatter, 1);
                }
                break;
            };
        }
        // 输出紧贴语句前的注释（leading comment）
        for (size_t i = leading_comment_index; i < stmt->start_token->index; i++) {
            DaiToken* token = DaiTokenList_get(formatter->tlist, i);
            Formatter_print_token(formatter, token);
        }
    } else {
        first_statement = false;
        // 第一条语句，直接输出
        DaiToken* prev_token = NULL;
        for (size_t i = 0; i < stmt->start_token->index; i++) {
            DaiToken* token = DaiTokenList_get(formatter->tlist, i);
            if (prev_token && token->start_line > prev_token->start_line + 1) {
                // 如果当前 token 的行号大于上一个 token 的行号，说明中间有空行
                Formatter_print_newline(formatter, 1);
            }
            prev_token = token;
            Formatter_print_token(formatter, token);
        }
    }
}

/*
处理语句后的空行和注释
只有紧贴着语句后面的注释才会处理
例如：
    语句1
    // 注释1，与语句之间没有空行

    // 注释2，与语句之间有空行
只需要处理紧贴着语句后面的注释1
*/
static void
Formatter_print_statement_trailing(Formatter* formatter, DaiAstStatement* stmt) {
    DaiToken* prev_token = formatter->last_token;
    DaiToken* token      = DaiTokenList_get(formatter->tlist, formatter->last_token->index + 1);
    while (token->type == DaiTokenType_comment && token->start_line == prev_token->start_line + 1) {
        Formatter_print_token(formatter, token);
        prev_token = token;
        token      = DaiTokenList_get(formatter->tlist, token->index + 1);
    }
}

static void
Formatter_print_statement(Formatter* formatter, DaiAstStatement* stmt) {
    if (stmt == NULL) {
        return;
    }
    Formatter_print_statement_leading(formatter, stmt);

    switch (stmt->type) {
        case DaiAstType_VarStatement: {
            DaiAstVarStatement* var_stmt = (DaiAstVarStatement*)stmt;
            // var/con
            Formatter_print_token_with_comment(formatter, var_stmt->start_token);
            Formatter_append_space(formatter);
            // identifier
            Formatter_print_expression(formatter, (DaiAstExpression*)var_stmt->name);
            // =
            Formatter_append_space(formatter);
            Formatter_print_next_token(formatter);
            Formatter_append_space(formatter);
            // value
            Formatter_print_expression(formatter, (DaiAstExpression*)var_stmt->value);
            // ;
            Formatter_print_next_token(formatter);
            break;
        }
        case DaiAstType_ReturnStatement: {
            DaiAstReturnStatement* return_stmt = (DaiAstReturnStatement*)stmt;
            // return
            Formatter_print_token_with_comment(formatter, return_stmt->start_token);
            if (return_stmt->return_value) {
                Formatter_append_space(formatter);
                // value
                Formatter_print_expression(formatter, return_stmt->return_value);
            }
            // ;
            Formatter_print_next_token(formatter);
            break;
        }
        case DaiAstType_ExpressionStatement: {
            DaiAstExpressionStatement* expr_stmt = (DaiAstExpressionStatement*)stmt;
            Formatter_print_expression(formatter, expr_stmt->expression);
            // ;
            Formatter_print_next_token(formatter);
            break;
        }
        case DaiAstType_IfStatement: {
            DaiAstIfStatement* if_stmt = (DaiAstIfStatement*)stmt;
            // if
            Formatter_print_token_with_comment(formatter, if_stmt->start_token);
            Formatter_append_space(formatter);
            // (
            Formatter_print_next_token_with_comment(formatter);
            // condition
            Formatter_print_expression(formatter, if_stmt->condition);
            // )
            Formatter_print_next_token_with_comment(formatter);
            Formatter_append_space(formatter);
            // body
            Formatter_print_block(formatter, if_stmt->then_branch);
            if (if_stmt->elif_branch_count > 0) {
                // elif
                for (size_t i = 0; i < if_stmt->elif_branch_count; i++) {
                    Formatter_print_comments(formatter);
                    Formatter_append_endline(formatter);
                    DaiBranch* branch = &(if_stmt->elif_branches[i]);
                    // elif
                    Formatter_print_next_token_with_comment(formatter);
                    Formatter_append_space(formatter);
                    // (
                    Formatter_print_next_token_with_comment(formatter);
                    // condition
                    Formatter_print_expression(formatter, branch->condition);
                    // )
                    Formatter_print_next_token_with_comment(formatter);
                    Formatter_append_space(formatter);
                    // body
                    Formatter_print_block(formatter, branch->then_branch);
                }
            }
            // else
            if (if_stmt->else_branch) {
                Formatter_print_comments(formatter);
                Formatter_append_endline(formatter);
                // else
                Formatter_print_next_token_with_comment(formatter);
                Formatter_append_space(formatter);
                // body
                Formatter_print_block(formatter, if_stmt->else_branch);
            }
            Formatter_skip_end_semicolon(formatter, stmt);
            break;
        }
        case DaiAstType_BlockStatement: {
            DaiAstBlockStatement* block_stmt = (DaiAstBlockStatement*)stmt;
            Formatter_print_block(formatter, block_stmt);
            Formatter_skip_end_semicolon(formatter, stmt);
            break;
        }
        case DaiAstType_AssignStatement: {
            DaiAstAssignStatement* assign_stmt = (DaiAstAssignStatement*)stmt;
            // left
            Formatter_print_expression(formatter, assign_stmt->left);
            Formatter_append_space(formatter);
            // =
            Formatter_print_next_token_with_comment(formatter);
            Formatter_append_space(formatter);
            // right
            Formatter_print_expression(formatter, assign_stmt->value);
            // ;
            Formatter_print_next_token(formatter);
            break;
        }
        case DaiAstType_FunctionStatement: {
            DaiAstFunctionStatement* func = (DaiAstFunctionStatement*)stmt;
            // fn
            Formatter_print_token_with_comment(formatter, func->start_token);
            Formatter_append_space(formatter);
            // name
            Formatter_print_next_token_with_comment(formatter);
            // (
            Formatter_print_next_token_with_comment(formatter);
            if (func->parameters_count > 0) {
                Formatter_indent(formatter);
                Formatter_append_endline(formatter);
                // 参数列表
                size_t defaults_count = DaiArray_length(func->defaults);
                for (size_t i = 0; i < func->parameters_count; i++) {
                    Formatter_print_expression(formatter, (DaiAstExpression*)func->parameters[i]);
                    if (i >= func->parameters_count - defaults_count) {
                        // =
                        Formatter_print_next_token_with_comment(formatter);
                        // 如果参数有默认值，则输出默认值
                        DaiAstExpression* default_expr = *(DaiAstExpression**)DaiArray_get(
                            func->defaults, i - (func->parameters_count - defaults_count));
                        // 默认值
                        Formatter_print_expression(formatter, default_expr);
                    }
                    // ,
                    if (Formatter_next_token_is(formatter, DaiTokenType_comma)) {
                        Formatter_print_next_token_with_comment(formatter);
                    } else {
                        Formatter_print(formatter, ",\n");
                    }
                    Formatter_append_endline(formatter);
                }
                Formatter_dedent(formatter);
            }
            // )
            Formatter_print_next_token_with_comment(formatter);
            Formatter_print(formatter, " ");
            // body
            Formatter_print_block(formatter, func->body);
            Formatter_skip_end_semicolon(formatter, stmt);
            break;
        }
        case DaiAstType_ClassStatement: {
            DaiAstClassStatement* class = (DaiAstClassStatement*)stmt;
            // class
            Formatter_print_token(formatter, class->start_token);
            Formatter_append_space(formatter);
            // name
            Formatter_print_next_token_with_comment(formatter);
            if (class->parent) {
                // (
                Formatter_print_next_token_with_comment(formatter);
                // parent
                Formatter_print_expression(formatter, (DaiAstExpression*)class->parent);
                // )
                Formatter_print_next_token_with_comment(formatter);
            }
            Formatter_append_space(formatter);
            // body
            Formatter_print_block(formatter, class->body);
            Formatter_skip_end_semicolon(formatter, stmt);
            break;
        }
        case DaiAstType_InsVarStatement: {
            DaiAstInsVarStatement* ins_var_stmt = (DaiAstInsVarStatement*)stmt;
            // var/con
            Formatter_print_token_with_comment(formatter, ins_var_stmt->start_token);
            Formatter_append_space(formatter);
            // name
            Formatter_print_next_token_with_comment(formatter);
            if (ins_var_stmt->value) {
                Formatter_append_space(formatter);
                // =
                Formatter_print_next_token_with_comment(formatter);
                Formatter_append_space(formatter);
                // value
                Formatter_print_expression(formatter, (DaiAstExpression*)ins_var_stmt->value);
            }
            // ;
            Formatter_print_next_token(formatter);
            break;
        }
        case DaiAstType_MethodStatement: {
            DaiAstMethodStatement* method = (DaiAstMethodStatement*)stmt;
            // fn
            Formatter_print_token_with_comment(formatter, method->start_token);
            Formatter_append_space(formatter);
            // name
            Formatter_print_next_token_with_comment(formatter);
            // (
            Formatter_print_next_token_with_comment(formatter);
            if (method->parameters_count > 0) {
                Formatter_indent(formatter);
                Formatter_append_endline(formatter);
                // 参数列表
                size_t defaults_count = DaiArray_length(method->defaults);
                for (size_t i = 0; i < method->parameters_count; i++) {
                    Formatter_print_expression(formatter, (DaiAstExpression*)method->parameters[i]);
                    if (i >= method->parameters_count - defaults_count) {
                        // =
                        Formatter_print_next_token_with_comment(formatter);
                        // 如果参数有默认值，则输出默认值
                        DaiAstExpression* default_expr = *(DaiAstExpression**)DaiArray_get(
                            method->defaults, i - (method->parameters_count - defaults_count));
                        // 默认值
                        Formatter_print_expression(formatter, default_expr);
                    }
                    // ,
                    if (Formatter_next_token_is(formatter, DaiTokenType_comma)) {
                        Formatter_print_next_token_with_comment(formatter);
                    } else {
                        Formatter_print(formatter, ",\n");
                    }
                    Formatter_append_endline(formatter);
                }
                Formatter_dedent(formatter);
            }
            // )
            Formatter_print_next_token_with_comment(formatter);
            Formatter_print(formatter, " ");
            // body
            Formatter_print_block(formatter, method->body);
            Formatter_skip_end_semicolon(formatter, stmt);
            break;
        }
        case DaiAstType_ClassVarStatement: {
            DaiAstClassVarStatement* class_var_stmt = (DaiAstClassVarStatement*)stmt;
            // class
            Formatter_print_token_with_comment(formatter, class_var_stmt->start_token);
            Formatter_append_space(formatter);
            // var/con
            Formatter_print_next_token_with_comment(formatter);
            Formatter_append_space(formatter);
            // name
            Formatter_print_expression(formatter, (DaiAstExpression*)class_var_stmt->name);
            Formatter_append_space(formatter);
            // =
            Formatter_print_next_token_with_comment(formatter);
            Formatter_append_space(formatter);
            // value
            Formatter_print_expression(formatter, (DaiAstExpression*)class_var_stmt->value);
            // ;
            Formatter_print_next_token(formatter);
            break;
        }
        case DaiAstType_ClassMethodStatement: {
            DaiAstClassMethodStatement* class_method = (DaiAstClassMethodStatement*)stmt;
            // class
            Formatter_print_token_with_comment(formatter, class_method->start_token);
            Formatter_append_space(formatter);
            // fn
            Formatter_print_next_token_with_comment(formatter);
            Formatter_append_space(formatter);
            // name
            Formatter_print_next_token_with_comment(formatter);
            // (
            Formatter_print_next_token_with_comment(formatter);
            if (class_method->parameters_count > 0) {
                Formatter_indent(formatter);
                Formatter_append_endline(formatter);
                // 参数列表
                size_t defaults_count = DaiArray_length(class_method->defaults);
                for (size_t i = 0; i < class_method->parameters_count; i++) {
                    Formatter_print_expression(formatter,
                                               (DaiAstExpression*)class_method->parameters[i]);
                    if (i >= class_method->parameters_count - defaults_count) {
                        // =
                        Formatter_print_next_token_with_comment(formatter);
                        // 如果参数有默认值，则输出默认值
                        DaiAstExpression* default_expr = *(DaiAstExpression**)DaiArray_get(
                            class_method->defaults,
                            i - (class_method->parameters_count - defaults_count));
                        // 默认值
                        Formatter_print_expression(formatter, default_expr);
                    }
                    // ,
                    if (Formatter_next_token_is(formatter, DaiTokenType_comma)) {
                        Formatter_print_next_token_with_comment(formatter);
                    } else {
                        Formatter_print(formatter, ",\n");
                    }
                    Formatter_append_endline(formatter);
                }
                Formatter_dedent(formatter);
            }
            // )
            Formatter_print_next_token_with_comment(formatter);
            Formatter_print(formatter, " ");
            // body
            Formatter_print_block(formatter, class_method->body);
            Formatter_skip_end_semicolon(formatter, stmt);
            break;
        }
        case DaiAstType_WhileStatement: {
            DaiAstWhileStatement* while_stmt = (DaiAstWhileStatement*)stmt;
            // while
            Formatter_print_token_with_comment(formatter, while_stmt->start_token);
            Formatter_append_space(formatter);
            // (
            Formatter_print_next_token_with_comment(formatter);
            // condition
            Formatter_print_expression(formatter, while_stmt->condition);
            // )
            Formatter_print_next_token_with_comment(formatter);
            Formatter_append_space(formatter);
            // body
            Formatter_print_block(formatter, while_stmt->body);
            Formatter_skip_end_semicolon(formatter, stmt);
            break;
        }
        case DaiAstType_ContinueStatement: {
            DaiAstContinueStatement* continue_stmt = (DaiAstContinueStatement*)stmt;
            // continue
            Formatter_print_token_with_comment(formatter, continue_stmt->start_token);
            // ;
            Formatter_print_next_token(formatter);
            break;
        }
        case DaiAstType_BreakStatement: {
            DaiAstBreakStatement* break_stmt = (DaiAstBreakStatement*)stmt;
            // break
            Formatter_print_token_with_comment(formatter, break_stmt->start_token);
            // ;
            Formatter_print_next_token(formatter);
            break;
        }
        case DaiAstType_ForInStatement: {
            DaiAstForInStatement* for_stmt = (DaiAstForInStatement*)stmt;
            // for
            Formatter_print_token_with_comment(formatter, for_stmt->start_token);
            Formatter_append_space(formatter);
            // (
            Formatter_print_next_token_with_comment(formatter);
            // var
            Formatter_print_next_token_with_comment(formatter);
            Formatter_append_space(formatter);
            if (for_stmt->i) {
                // identifier(i)
                Formatter_print_expression(formatter, (DaiAstExpression*)for_stmt->i);
                // ,
                Formatter_print_next_token_with_comment(formatter);
                Formatter_append_space(formatter);
            }
            // identifier(e)
            Formatter_print_expression(formatter, (DaiAstExpression*)for_stmt->e);
            Formatter_append_space(formatter);
            // in
            Formatter_print_next_token_with_comment(formatter);
            Formatter_append_space(formatter);
            // iterable
            Formatter_print_expression(formatter, for_stmt->expression);
            // )
            Formatter_print_next_token_with_comment(formatter);
            Formatter_append_space(formatter);
            // body
            Formatter_print_block(formatter, for_stmt->body);
            Formatter_skip_end_semicolon(formatter, stmt);
            break;
        }
        default: unreachable();
    }
    // 输出语句后的行尾注释
    if (stmt->end_token->type == DaiTokenType_comment &&
        stmt->end_token->start_line == formatter->last_token->start_line) {
        Formatter_print(formatter, "  ");
        Formatter_print_token(formatter, stmt->end_token);
    } else {
        // 如果语句后面没有换行，添加一个换行符
        Formatter_append_endline(formatter);
    }

    Formatter_print_statement_trailing(formatter, stmt);
}

static void
Formatter_run(Formatter* formatter) {
    for (int i = 0; i < formatter->program->length; i++) {
        Formatter_print_statement(formatter, formatter->program->statements[i]);
    }

    // 处理文件末尾的注释
    Formatter_print_comments(formatter);
}

char*
dai_fmt(const DaiAstProgram* program, size_t source_len) {
    // 初始化格式化器和字符串缓冲区
    Formatter formatter;
    Formatter_init(&formatter, program);
    DaiStringBuffer_grow(formatter.sb, source_len + 32);   // 32 是拍脑袋定的
    // 开始格式化
    Formatter_run(&formatter);
    // 保证格式化代码末尾有换行符
    Formatter_append_endline(&formatter);
    // 获取格式化后的字符串
    char* formatted = Formatter_get_string(&formatter);
    // 释放格式化器
    Formatter_reset(&formatter);
    return formatted;
}