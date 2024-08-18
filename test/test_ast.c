#include "munit/munit.h"

#include "dai_ast.h"
#include "dai_malloc.h"

static MunitResult
test_ast_type_string(__attribute__((unused)) const MunitParameter params[],
                     __attribute__((unused)) void* user_data) {
    munit_assert_string_equal(DaiAstType_string(DaiAstType_unspecified), "DaiAstType_unspecified");
    munit_assert_string_equal(DaiAstType_string(DaiAstType_program), "DaiAstType_program");
    munit_assert_string_equal(DaiAstType_string(DaiAstType_StatementStart),
                              "DaiAstType_StatementStart");
    munit_assert_string_equal(DaiAstType_string(DaiAstType_VarStatement),
                              "DaiAstType_VarStatement");
    munit_assert_string_equal(DaiAstType_string(DaiAstType_ReturnStatement),
                              "DaiAstType_ReturnStatement");
    munit_assert_string_equal(DaiAstType_string(DaiAstType_ExpressionStatement),
                              "DaiAstType_ExpressionStatement");
    munit_assert_string_equal(DaiAstType_string(DaiAstType_IfStatement), "DaiAstType_IfStatement");
    munit_assert_string_equal(DaiAstType_string(DaiAstType_BlockStatement),
                              "DaiAstType_BlockStatement");
    munit_assert_string_equal(DaiAstType_string(DaiAstType_StatementEnd),
                              "DaiAstType_StatementEnd");
    munit_assert_string_equal(DaiAstType_string(DaiAstType_ExpressionStart),
                              "DaiAstType_ExpressionStart");
    munit_assert_string_equal(DaiAstType_string(DaiAstType_IntegerLiteral),
                              "DaiAstType_IntegerLiteral");
    munit_assert_string_equal(DaiAstType_string(DaiAstType_Boolean), "DaiAstType_Boolean");
    munit_assert_string_equal(DaiAstType_string(DaiAstType_Identifier), "DaiAstType_Identifier");
    munit_assert_string_equal(DaiAstType_string(DaiAstType_PrefixExpression),
                              "DaiAstType_PrefixExpression");
    munit_assert_string_equal(DaiAstType_string(DaiAstType_InfixExpression),
                              "DaiAstType_InfixExpression");
    munit_assert_string_equal(DaiAstType_string(DaiAstType_FunctionLiteral),
                              "DaiAstType_FunctionLiteral");
    munit_assert_string_equal(DaiAstType_string(DaiAstType_CallExpression),
                              "DaiAstType_CallExpression");
    munit_assert_string_equal(DaiAstType_string(DaiAstType_ExpressionEnd),
                              "DaiAstType_ExpressionEnd");
    return MUNIT_OK;
}

static MunitResult
test_ast_node_string(__attribute__((unused)) const MunitParameter params[],
                     __attribute__((unused)) void* user_data) {
    DaiToken token               = {DaiTokenType_ident, "foo", 0, 0, 0, 0};
    DaiAstIdentifier* ident      = DaiAstIdentifier_New(&token);
    DaiToken token2              = {DaiTokenType_int, "123", 0, 0, 0, 0};
    DaiAstIntegerLiteral* num    = DaiAstIntegerLiteral_New(&token2);
    DaiAstVarStatement* var_stmt = DaiAstVarStatement_New(ident, (DaiAstExpression*)num);
    for (int i = 0; i < 10; i++) {
        char* s;
        s = var_stmt->string_fn((DaiAstBase*)var_stmt, false);
        dai_free(s);
        s = var_stmt->string_fn((DaiAstBase*)var_stmt, true);
        dai_free(s);
    }
    var_stmt->free_fn((DaiAstBase*)var_stmt, true);
    return MUNIT_OK;
}

MunitTest ast_tests[] = {
    {(char*)"/test_ast_type_string",
     test_ast_type_string,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/test_ast_node_string",
     test_ast_node_string,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};
