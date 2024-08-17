#ifndef F01FB03C_21BA_4B8D_8A3A_2ED103BA541C
#define F01FB03C_21BA_4B8D_8A3A_2ED103BA541C

#include "dai_ast/dai_astexpression.h"
#include "dai_ast/dai_astidentifier.h"

typedef struct {
    DAI_AST_EXPRESSION_HEAD
    DaiAstExpression* left;
    DaiAstIdentifier* name;
} DaiAstDotExpression;

DaiAstDotExpression*
DaiAstDotExpression_New(DaiAstExpression* left);

#endif /* F01FB03C_21BA_4B8D_8A3A_2ED103BA541C */
