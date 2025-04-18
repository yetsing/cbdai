#ifndef A27DBEBF_6C4E_4D54_AFEF_42B0BCF50BC5
#define A27DBEBF_6C4E_4D54_AFEF_42B0BCF50BC5

typedef enum {
    DaiAstType_unspecified = 0,
    DaiAstType_program,
    // #region statement
    DaiAstType_StatementStart,
    DaiAstType_VarStatement,
    DaiAstType_ReturnStatement,
    DaiAstType_ExpressionStatement,
    DaiAstType_IfStatement,
    DaiAstType_BlockStatement,
    DaiAstType_AssignStatement,
    DaiAstType_FunctionStatement,
    DaiAstType_ClassStatement,
    DaiAstType_InsVarStatement,
    DaiAstType_MethodStatement,
    DaiAstType_ClassVarStatement,
    DaiAstType_ClassMethodStatement,
    DaiAstType_WhileStatement,
    DaiAstType_ContinueStatement,
    DaiAstType_BreakStatement,
    DaiAstType_ForInStatement,
    DaiAstType_StatementEnd,
    // #endregion

    // #region expression
    DaiAstType_ExpressionStart,
    DaiAstType_IntegerLiteral,
    DaiAstType_FloatLiteral,
    DaiAstType_Boolean,
    DaiAstType_Nil,
    DaiAstType_Identifier,
    DaiAstType_PrefixExpression,
    DaiAstType_InfixExpression,
    DaiAstType_FunctionLiteral,
    DaiAstType_StringLiteral,
    DaiAstType_ArrayLiteral,
    DaiAstType_CallExpression,
    DaiAstType_DotExpression,
    DaiAstType_SelfExpression,
    DaiAstType_ClassExpression,
    DaiAstType_SuperExpression,
    DaiAstType_SubscriptExpression,
    DaiAstType_MapLiteral,
    DaiAstType_ExpressionEnd,
    // #endregion
} DaiAstType;

const char*
DaiAstType_string(DaiAstType type);

#endif /* A27DBEBF_6C4E_4D54_AFEF_42B0BCF50BC5 */
