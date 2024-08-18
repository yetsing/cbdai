#ifndef B631E35D_4FD8_4671_8307_6575CED881B1
#define B631E35D_4FD8_4671_8307_6575CED881B1

#include <stddef.h>

#include "dai_ast/dai_astexpression.h"

typedef struct {
    DAI_AST_EXPRESSION_HEAD
    DaiAstExpression* function;
    size_t arguments_count;
    DaiAstExpression** arguments;
} DaiAstCallExpression;

DaiAstCallExpression*
DaiAstCallExpression_New(void);

#endif /* B631E35D_4FD8_4671_8307_6575CED881B1 */
