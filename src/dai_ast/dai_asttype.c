#include "dai_ast/dai_asttype.h"


static const char* DaiAstTypeStrings[] = {
    "DaiAstType_unspecified",
    "DaiAstType_program",
    "DaiAstType_StatementStart",
    "DaiAstType_VarStatement",
    "DaiAstType_ReturnStatement",
    "DaiAstType_ExpressionStatement",
    "DaiAstType_IfStatement",
    "DaiAstType_BlockStatement",
    "DaiAstType_AssignStatement",
    "DaiAstType_FunctionStatement",
    "DaiAstType_ClassStatement",
    "DaiAstType_InsVarStatement",
    "DaiAstType_MethodStatement",
    "DaiAstType_ClassVarStatement",
    "DaiAstType_ClassMethodStatement",
    "DaiAstType_WhileStatement",
    "DaiAstType_ContinueStatement",
    "DaiAstType_BreakStatement",
    "DaiAstType_ForInStatement",
    "DaiAstType_StatementEnd",
    "DaiAstType_ExpressionStart",
    "DaiAstType_IntegerLiteral",
    "DaiAstType_FloatLiteral",
    "DaiAstType_Boolean",
    "DaiAstType_Nil",
    "DaiAstType_Identifier",
    "DaiAstType_PrefixExpression",
    "DaiAstType_InfixExpression",
    "DaiAstType_FunctionLiteral",
    "DaiAstType_StringLiteral",
    "DaiAstType_ArrayLiteral",
    "DaiAstType_CallExpression",
    "DaiAstType_DotExpression",
    "DaiAstType_SelfExpression",
    "DaiAstType_ClassExpression",
    "DaiAstType_SuperExpression",
    "DaiAstType_SubscriptExpression",
    "DaiAstType_MapLiteral",
    "DaiAstType_ExpressionEnd",
};

const char*
DaiAstType_string(const DaiAstType type) {
    return DaiAstTypeStrings[type];
}
