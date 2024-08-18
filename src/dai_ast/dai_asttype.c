#include "dai_ast/dai_asttype.h"


static const char* AstTypeStrings[] = {
    "DaiAstType_unspecified",          "DaiAstType_program",
    "DaiAstType_StatementStart",       "DaiAstType_VarStatement",
    "DaiAstType_ReturnStatement",      "DaiAstType_ExpressionStatement",
    "DaiAstType_IfStatement",          "DaiAstType_BlockStatement",
    "DaiAstType_AssignStatement",      "DaiAstType_FunctionStatement",
    "DaiAstType_ClassStatement",       "DaiAstType_InsVarStatement",
    "DaiAstType_MethodStatement",      "DaiAstType_ClassVarStatement",
    "DaiAstType_ClassMethodStatement", "DaiAstType_WhileStatement",
    "DaiAstType_ContinueStatement",    "DaiAstType_BreakStatement",
    "DaiAstType_StatementEnd",         "DaiAstType_ExpressionStart",
    "DaiAstType_IntegerLiteral",       "DaiAstType_Boolean",
    "DaiAstType_Identifier",           "DaiAstType_PrefixExpression",
    "DaiAstType_InfixExpression",      "DaiAstType_FunctionLiteral",
    "DaiAstType_StringLiteral",        "DaiAstType_CallExpression",
    "DaiAstType_DotExpression",        "DaiAstType_SelfExpression",
    "DaiAstType_SuperExpression",      "DaiAstType_ExpressionEnd",

};
const char*
DaiAstType_string(DaiAstType type) {
    return AstTypeStrings[type];
}
