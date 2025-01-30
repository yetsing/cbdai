#include <limits.h>
#include <stdio.h>

#include "dai_ast/dai_astexpression.h"
#include "dai_ast/dai_asttype.h"
#include "munit/munit.h"

#include "dai_common.h"
#include "dai_malloc.h"
#include "dai_parse.h"
#include "dai_tokenize.h"
#include "dai_utils.h"


// 获取当前文件所在文件夹路径
static void
get_file_directory(char* path) {
    if (realpath(__FILE__, path) == NULL) {
        perror("realpath");
        assert(false);
    }
    char* last_slash = strrchr(path, '/');
    if (last_slash) {
        *(last_slash + 1) = '\0';
    }
}

// #region 测试辅助函数

static void
check_var_statement(DaiAstStatement* stmt, const char* name) {
    munit_assert_int(stmt->type, ==, DaiAstType_VarStatement);
    DaiAstVarStatement* var_stmt = (DaiAstVarStatement*)stmt;
    munit_assert_string_equal(var_stmt->name->value, name);
    DaiAstIdentifier* ident = var_stmt->name;
    munit_assert_int(ident->start_line, ==, ident->end_line);
    munit_assert_int(ident->start_column + strlen(name), ==, ident->end_column);
}

static void
check_ins_var_statement(DaiAstInsVarStatement* stmt, const char* name) {
    munit_assert_int(stmt->type, ==, DaiAstType_InsVarStatement);
    DaiAstInsVarStatement* var_stmt = (DaiAstInsVarStatement*)stmt;
    munit_assert_string_equal(var_stmt->name->value, name);
    DaiAstIdentifier* ident = var_stmt->name;
    munit_assert_int(ident->start_line, ==, ident->end_line);
    munit_assert_int(ident->start_column + strlen(name), ==, ident->end_column);
}

static void
check_class_var_statement(DaiAstClassVarStatement* stmt, const char* name) {
    munit_assert_int(stmt->type, ==, DaiAstType_ClassVarStatement);
    DaiAstClassVarStatement* var_stmt = (DaiAstClassVarStatement*)stmt;
    munit_assert_string_equal(var_stmt->name->value, name);
    DaiAstIdentifier* ident = var_stmt->name;
    munit_assert_int(ident->start_line, ==, ident->end_line);
    munit_assert_int(ident->start_column + strlen(name), ==, ident->end_column);
}

static void
check_integer_literal(DaiAstExpression* expr, int64_t value) {
    DaiAstIntegerLiteral* integ = (DaiAstIntegerLiteral*)expr;
    munit_assert_int(integ->type, ==, DaiAstType_IntegerLiteral);
    munit_assert_int(integ->value, ==, value);
    munit_assert_int(integ->start_line, ==, integ->end_line);
    munit_assert_int(integ->start_column + strlen(integ->literal), ==, integ->end_column);
}

static void
check_string_literal(DaiAstExpression* expr, const char* value) {
    DaiAstStringLiteral* str = (DaiAstStringLiteral*)expr;
    munit_assert_int(str->type, ==, DaiAstType_StringLiteral);
    munit_assert_string_equal(str->value, value);
    munit_assert_int(str->start_line, ==, str->end_line);
    munit_assert_int(str->start_column + strlen(value), ==, str->end_column);
}

static void
check_boolean_literal(DaiAstExpression* expr, bool value) {
    DaiAstBoolean* boolean = (DaiAstBoolean*)expr;
    munit_assert_int(boolean->type, ==, DaiAstType_Boolean);
    munit_assert_int(boolean->value, ==, value);
    munit_assert_int(boolean->start_line, ==, boolean->end_line);
    if (boolean->value) {
        munit_assert_int(boolean->start_column + 4, ==, boolean->end_column);
    } else {
        munit_assert_int(boolean->start_column + 5, ==, boolean->end_column);
    }
}

static void
check_identifier(DaiAstExpression* expr, const char* value) {
    DaiAstIdentifier* ident = (DaiAstIdentifier*)expr;
    munit_assert_int(ident->type, ==, DaiAstType_Identifier);
    munit_assert_string_equal(ident->value, value);
    munit_assert_int(ident->start_line, ==, ident->end_line);
    munit_assert_int(ident->start_column + strlen(value), ==, ident->end_column);
}

typedef enum {
    ExpectedValueType_int64  = 0,
    ExpectedValueType_string = 1,
    ExpectedValueType_bool,
} ExpectedValueType;

static void
check_literal_expression(DaiAstExpression* expr, void* expected, ExpectedValueType type) {
    switch (type) {
        case ExpectedValueType_int64: {
            check_integer_literal(expr, (int64_t)expected);
            break;
        }
        case ExpectedValueType_string: {
            check_identifier(expr, (char*)expected);
            break;
        }
        case ExpectedValueType_bool: {
            check_boolean_literal(expr, (bool)expected);
            break;
        }

        default: {
            // unreachable
            munit_assert(false);
            break;
        }
    }
}

static void
check_infix_expression(DaiAstExpression* expr, void* left, ExpectedValueType left_type,
                       const char* op, void* right, ExpectedValueType right_type) {
    DaiAstInfixExpression* infix = (DaiAstInfixExpression*)expr;
    munit_assert_int(infix->type, ==, DaiAstType_InfixExpression);
    munit_assert_string_equal(infix->operator, op);
    check_literal_expression(infix->left, left, left_type);
    check_literal_expression(infix->right, right, right_type);
}

#define check_infix_expression_int64(expr, left, op, right) \
    check_infix_expression(                                 \
        expr, (void*)left, ExpectedValueType_int64, op, (void*)right, ExpectedValueType_int64)
#define check_infix_expression_string(expr, left, op, right) \
    check_infix_expression(                                  \
        expr, left, ExpectedValueType_string, op, right, ExpectedValueType_string)

static void
parse_helper(const char* input, DaiAstProgram* program) {
    DaiTokenList* tlist = DaiTokenList_New();
    DaiSyntaxError* err = dai_tokenize_string(input, tlist);
    if (err != NULL) {
        DaiSyntaxError_setFilename(err, "<test>");
        DaiSyntaxError_pprint(err, input);
    }
    munit_assert_null(err);
    err = dai_parse(tlist, program);
    if (err != NULL) {
        DaiSyntaxError_setFilename(err, "<test>");
        DaiSyntaxError_pprint(err, input);
    }
    munit_assert_null(err);
    DaiTokenList_free(tlist);
}

static DaiSyntaxError*
parse_error(const char* input) {
    DaiTokenList* tlist = DaiTokenList_New();
    DaiSyntaxError* err = dai_tokenize_string(input, tlist);
    if (err != NULL) {
        DaiSyntaxError_setFilename(err, "<test>");
        DaiSyntaxError_pprint(err, input);
    }
    munit_assert_null(err);
    DaiAstProgram prog;
    DaiAstProgram_init(&prog);
    DaiAstProgram* program = &prog;
    err                    = dai_parse(tlist, program);
    if (err) {
        DaiSyntaxError_setFilename(err, "<test>");
    }
    DaiTokenList_free(tlist);
    program->free_fn((DaiAstBase*)program, true);
    return err;
}

// #endregion

static MunitResult
test_var_statements(__attribute__((unused)) const MunitParameter params[],
                    __attribute__((unused)) void* user_data) {
    char* input = "var x = 5;\n"
                  "var y = 10;\n"
                  "var foobar = 838383;\n"
                  "";
    DaiAstProgram prog;
    DaiAstProgram_init(&prog);
    DaiAstProgram* program = &prog;
    parse_helper(input, program);

    munit_assert_int(program->length, ==, 3);

    struct TestCase {
        const char* expectedIndentifier;
        int start_line;
        int start_column;
        int end_line;
        int end_column;
    };
    struct TestCase tests[] = {
        {"x", 1, 1, 1, 11},
        {"y", 2, 1, 2, 12},
        {"foobar", 3, 1, 3, 21},
    };
    for (int i = 0; i < program->length; i++) {
        check_var_statement(program->statements[i], tests[i].expectedIndentifier);
        if (tests[i].start_line != 0) {
            munit_assert_int(program->statements[i]->start_line, ==, tests[i].start_line);
            munit_assert_int(program->statements[i]->start_column, ==, tests[i].start_column);
            munit_assert_int(program->statements[i]->end_line, ==, tests[i].end_line);
            munit_assert_int(program->statements[i]->end_column, ==, tests[i].end_column);
        }
    }
    program->free_fn((DaiAstBase*)program, true);

    struct {
        const char* input;
        const char* expected_identifier;
        ExpectedValueType expected_value_type;
        void* expected_value;
    } tests2[] = {
        {"var x = 5;", "x", ExpectedValueType_int64, (void*)5},
        {"var y = true;", "y", ExpectedValueType_bool, (void*)true},
        {"var foobar = y;", "foobar", ExpectedValueType_string, "y"},
    };
    for (int i = 0; i < sizeof(tests2) / sizeof(tests2[0]); i++) {
        parse_helper(tests2[i].input, program);

        munit_assert_int(program->length, ==, 1);
        DaiAstVarStatement* var_stmt = (DaiAstVarStatement*)program->statements[0];
        check_var_statement((DaiAstStatement*)var_stmt, tests2[i].expected_identifier);
        check_literal_expression(
            var_stmt->value, tests2[i].expected_value, tests2[i].expected_value_type);
        program->free_fn((DaiAstBase*)program, true);
    }
    return MUNIT_OK;
}

static MunitResult
test_return_statements(__attribute__((unused)) const MunitParameter params[],
                       __attribute__((unused)) void* user_data) {
    const char* input = ""
                        "return 5;\n"
                        "return 10;\n"
                        "return 993;\n"
                        "return ;\n"
                        "";
    DaiAstProgram prog;
    DaiAstProgram_init(&prog);
    DaiAstProgram* program = &prog;
    parse_helper(input, program);

    munit_assert_int(program->length, ==, 4);
    int end_columns[4] = {10, 11, 12, 9};
    for (int i = 0; i < program->length; i++) {
        munit_assert_int(program->statements[i]->type, ==, DaiAstType_ReturnStatement);
        munit_assert_int(program->statements[i]->start_line, ==, i + 1);
        munit_assert_int(program->statements[i]->start_column, ==, 1);
        munit_assert_int(program->statements[i]->end_line, ==, i + 1);
        munit_assert_int(program->statements[i]->end_column, ==, end_columns[i]);
    }
    program->free_fn((DaiAstBase*)program, true);

    struct {
        const char* input;
        ExpectedValueType expected_value_type;
        void* expected_value;
    } tests[] = {
        {"return 5;", ExpectedValueType_int64, (void*)5},
        {"return true;", ExpectedValueType_bool, (void*)true},
        {"return foobar;", ExpectedValueType_string, "foobar"},
    };
    for (int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        parse_helper(tests[i].input, program);

        munit_assert_int(program->length, ==, 1);
        DaiAstReturnStatement* ret_stmt = (DaiAstReturnStatement*)program->statements[0];
        check_literal_expression(
            ret_stmt->return_value, tests[i].expected_value, tests[i].expected_value_type);
        program->free_fn((DaiAstBase*)program, true);
    }
    return MUNIT_OK;
}

static MunitResult
test_if_statements(__attribute__((unused)) const MunitParameter params[],
                   __attribute__((unused)) void* user_data) {
    const char* input = "if (x < y) { x;};";
    DaiAstProgram prog;
    DaiAstProgram_init(&prog);
    DaiAstProgram* program = &prog;
    parse_helper(input, program);

    munit_assert_int(program->length, ==, 1);
    munit_assert_int(program->statements[0]->type, ==, DaiAstType_IfStatement);

    DaiAstIfStatement* ifstatement = (DaiAstIfStatement*)program->statements[0];
    {
        munit_assert_int(ifstatement->start_line, ==, 1);
        munit_assert_int(ifstatement->start_column, ==, 1);
        munit_assert_int(ifstatement->end_line, ==, 1);
        munit_assert_int(ifstatement->end_column, ==, 18);
    }
    check_infix_expression_string(ifstatement->condition, "x", "<", "y");
    {
        munit_assert_int(ifstatement->condition->start_line, ==, 1);
        munit_assert_int(ifstatement->condition->start_column, ==, 5);
        munit_assert_int(ifstatement->condition->end_line, ==, 1);
        munit_assert_int(ifstatement->condition->end_column, ==, 10);
    }

    munit_assert_int(ifstatement->then_branch->type, ==, DaiAstType_BlockStatement);
    munit_assert_int(ifstatement->then_branch->length, ==, 1);
    DaiAstExpressionStatement* stmt =
        (DaiAstExpressionStatement*)ifstatement->then_branch->statements[0];
    munit_assert_int(stmt->type, ==, DaiAstType_ExpressionStatement);
    check_identifier(stmt->expression, "x");

    munit_assert_null(ifstatement->else_branch);

    program->free_fn((DaiAstBase*)program, true);
    return MUNIT_OK;
}

static MunitResult
test_if_else_statements(__attribute__((unused)) const MunitParameter params[],
                        __attribute__((unused)) void* user_data) {
    const char* input = "if (x < y) { x;} else {y;};";
    DaiAstProgram prog;
    DaiAstProgram_init(&prog);
    DaiAstProgram* program = &prog;
    parse_helper(input, program);

    munit_assert_int(program->length, ==, 1);
    munit_assert_int(program->statements[0]->type, ==, DaiAstType_IfStatement);

    DaiAstIfStatement* ifstatement = (DaiAstIfStatement*)program->statements[0];
    {
        munit_assert_int(ifstatement->start_line, ==, 1);
        munit_assert_int(ifstatement->start_column, ==, 1);
        munit_assert_int(ifstatement->end_line, ==, 1);
        munit_assert_int(ifstatement->end_column, ==, 28);
    }
    check_infix_expression_string(ifstatement->condition, "x", "<", "y");

    munit_assert_int(ifstatement->then_branch->type, ==, DaiAstType_BlockStatement);
    munit_assert_int(ifstatement->then_branch->length, ==, 1);
    DaiAstExpressionStatement* stmt =
        (DaiAstExpressionStatement*)ifstatement->then_branch->statements[0];
    munit_assert_int(stmt->type, ==, DaiAstType_ExpressionStatement);
    check_identifier(stmt->expression, "x");

    munit_assert_int(ifstatement->else_branch->type, ==, DaiAstType_BlockStatement);
    munit_assert_int(ifstatement->else_branch->length, ==, 1);
    stmt = (DaiAstExpressionStatement*)ifstatement->else_branch->statements[0];
    munit_assert_int(stmt->type, ==, DaiAstType_ExpressionStatement);
    check_identifier(stmt->expression, "y");

    program->free_fn((DaiAstBase*)program, true);
    return MUNIT_OK;
}

static MunitResult
test_if_elif_else_statements(__attribute__((unused)) const MunitParameter params[],
                             __attribute__((unused)) void* user_data) {
    const char* input = "if (x < y) { x;} "
                        "elif (x > y) { y;}"
                        "else {y;};";
    DaiAstProgram prog;
    DaiAstProgram_init(&prog);
    DaiAstProgram* program = &prog;
    parse_helper(input, program);

    munit_assert_int(program->length, ==, 1);
    munit_assert_int(program->statements[0]->type, ==, DaiAstType_IfStatement);

    DaiAstIfStatement* ifstatement = (DaiAstIfStatement*)program->statements[0];
    {
        munit_assert_int(ifstatement->start_line, ==, 1);
        munit_assert_int(ifstatement->start_column, ==, 1);
        munit_assert_int(ifstatement->end_line, ==, 1);
        munit_assert_int(ifstatement->end_column, ==, strlen(input) + 1);
    }
    check_infix_expression_string(ifstatement->condition, "x", "<", "y");

    munit_assert_int(ifstatement->then_branch->type, ==, DaiAstType_BlockStatement);
    munit_assert_int(ifstatement->then_branch->length, ==, 1);
    DaiAstExpressionStatement* stmt =
        (DaiAstExpressionStatement*)ifstatement->then_branch->statements[0];
    munit_assert_int(stmt->type, ==, DaiAstType_ExpressionStatement);
    check_identifier(stmt->expression, "x");

    munit_assert_int(ifstatement->elif_branch_count, ==, 1);
    {
        DaiBranch branch = ifstatement->elif_branches[0];
        check_infix_expression_string(branch.condition, "x", ">", "y");
        munit_assert_int(branch.then_branch->length, ==, 1);
        stmt = (DaiAstExpressionStatement*)branch.then_branch->statements[0];
        munit_assert_int(stmt->type, ==, DaiAstType_ExpressionStatement);
        check_identifier(stmt->expression, "y");
    }

    munit_assert_int(ifstatement->else_branch->type, ==, DaiAstType_BlockStatement);
    munit_assert_int(ifstatement->else_branch->length, ==, 1);
    stmt = (DaiAstExpressionStatement*)ifstatement->else_branch->statements[0];
    munit_assert_int(stmt->type, ==, DaiAstType_ExpressionStatement);
    check_identifier(stmt->expression, "y");

    program->free_fn((DaiAstBase*)program, true);
    return MUNIT_OK;
}

static MunitResult
test_while_statements(__attribute__((unused)) const MunitParameter params[],
                      __attribute__((unused)) void* user_data) {
    const char* input = "while (x < y) {\n"
                        " x;\n"
                        " continue;\n"
                        " break;\n"
                        "}";
    DaiAstProgram prog;
    DaiAstProgram_init(&prog);
    DaiAstProgram* program = &prog;
    parse_helper(input, program);

    munit_assert_int(program->length, ==, 1);
    munit_assert_int(program->statements[0]->type, ==, DaiAstType_WhileStatement);

    DaiAstWhileStatement* while_stmt = (DaiAstWhileStatement*)program->statements[0];
    {
        munit_assert_int(while_stmt->start_line, ==, 1);
        munit_assert_int(while_stmt->start_column, ==, 1);
        munit_assert_int(while_stmt->end_line, ==, 5);
        munit_assert_int(while_stmt->end_column, ==, 2);
    }
    check_infix_expression_string(while_stmt->condition, "x", "<", "y");

    munit_assert_int(while_stmt->body->type, ==, DaiAstType_BlockStatement);
    munit_assert_int(while_stmt->body->length, ==, 3);
    {
        DaiAstExpressionStatement* stmt =
            (DaiAstExpressionStatement*)while_stmt->body->statements[0];
        munit_assert_int(stmt->type, ==, DaiAstType_ExpressionStatement);
        check_identifier(stmt->expression, "x");
    }
    {
        DaiAstContinueStatement* stmt = (DaiAstContinueStatement*)while_stmt->body->statements[1];
        munit_assert_int(stmt->type, ==, DaiAstType_ContinueStatement);
    }
    {
        DaiAstBreakStatement* stmt = (DaiAstBreakStatement*)while_stmt->body->statements[2];
        munit_assert_int(stmt->type, ==, DaiAstType_BreakStatement);
    }

    program->free_fn((DaiAstBase*)program, true);
    return MUNIT_OK;
}

static MunitResult
test_assign_statements(__attribute__((unused)) const MunitParameter params[],
                       __attribute__((unused)) void* user_data) {
    {
        const char* input = "x = y;";
        DaiAstProgram prog;
        DaiAstProgram_init(&prog);
        DaiAstProgram* program = &prog;
        parse_helper(input, program);
        munit_assert_int(program->length, ==, 1);
        munit_assert_int(program->statements[0]->type, ==, DaiAstType_AssignStatement);
        DaiAstAssignStatement* stmt = (DaiAstAssignStatement*)program->statements[0];
        munit_assert_int(stmt->type, ==, DaiAstType_AssignStatement);
        check_identifier(stmt->left, "x");
        munit_assert_int(stmt->start_line, ==, 1);
        munit_assert_int(stmt->start_column, ==, 1);
        munit_assert_int(stmt->end_line, ==, 1);
        munit_assert_int(stmt->end_column, ==, 7);
        munit_assert_null(stmt->operator);
        check_identifier(stmt->value, "y");
        program->free_fn((DaiAstBase*)program, true);
    }
    {
        const char* input = "x += y;";
        DaiAstProgram prog;
        DaiAstProgram_init(&prog);
        DaiAstProgram* program = &prog;
        parse_helper(input, program);
        munit_assert_int(program->length, ==, 1);
        munit_assert_int(program->statements[0]->type, ==, DaiAstType_AssignStatement);
        DaiAstAssignStatement* stmt = (DaiAstAssignStatement*)program->statements[0];
        munit_assert_int(stmt->type, ==, DaiAstType_AssignStatement);
        check_identifier(stmt->left, "x");
        munit_assert_int(stmt->start_line, ==, 1);
        munit_assert_int(stmt->start_column, ==, 1);
        munit_assert_int(stmt->end_line, ==, 1);
        munit_assert_int(stmt->end_column, ==, 8);
        munit_assert_string_equal(stmt->operator, "+");
        check_identifier(stmt->value, "y");
        program->free_fn((DaiAstBase*)program, true);
    }
    {
        const char* input = "x -= y;";
        DaiAstProgram prog;
        DaiAstProgram_init(&prog);
        DaiAstProgram* program = &prog;
        parse_helper(input, program);
        munit_assert_int(program->length, ==, 1);
        munit_assert_int(program->statements[0]->type, ==, DaiAstType_AssignStatement);
        DaiAstAssignStatement* stmt = (DaiAstAssignStatement*)program->statements[0];
        munit_assert_int(stmt->type, ==, DaiAstType_AssignStatement);
        check_identifier(stmt->left, "x");
        munit_assert_int(stmt->start_line, ==, 1);
        munit_assert_int(stmt->start_column, ==, 1);
        munit_assert_int(stmt->end_line, ==, 1);
        munit_assert_int(stmt->end_column, ==, 8);
        munit_assert_string_equal(stmt->operator, "-");
        check_identifier(stmt->value, "y");
        program->free_fn((DaiAstBase*)program, true);
    }
    {
        const char* input = "x *= y;";
        DaiAstProgram prog;
        DaiAstProgram_init(&prog);
        DaiAstProgram* program = &prog;
        parse_helper(input, program);
        munit_assert_int(program->length, ==, 1);
        munit_assert_int(program->statements[0]->type, ==, DaiAstType_AssignStatement);
        DaiAstAssignStatement* stmt = (DaiAstAssignStatement*)program->statements[0];
        munit_assert_int(stmt->type, ==, DaiAstType_AssignStatement);
        check_identifier(stmt->left, "x");
        munit_assert_int(stmt->start_line, ==, 1);
        munit_assert_int(stmt->start_column, ==, 1);
        munit_assert_int(stmt->end_line, ==, 1);
        munit_assert_int(stmt->end_column, ==, 8);
        munit_assert_string_equal(stmt->operator, "*");
        check_identifier(stmt->value, "y");
        program->free_fn((DaiAstBase*)program, true);
    }
    {
        const char* input = "x /= y;";
        DaiAstProgram prog;
        DaiAstProgram_init(&prog);
        DaiAstProgram* program = &prog;
        parse_helper(input, program);
        munit_assert_int(program->length, ==, 1);
        munit_assert_int(program->statements[0]->type, ==, DaiAstType_AssignStatement);
        DaiAstAssignStatement* stmt = (DaiAstAssignStatement*)program->statements[0];
        munit_assert_int(stmt->type, ==, DaiAstType_AssignStatement);
        check_identifier(stmt->left, "x");
        munit_assert_int(stmt->start_line, ==, 1);
        munit_assert_int(stmt->start_column, ==, 1);
        munit_assert_int(stmt->end_line, ==, 1);
        munit_assert_int(stmt->end_column, ==, 8);
        munit_assert_string_equal(stmt->operator, "/");
        check_identifier(stmt->value, "y");
        program->free_fn((DaiAstBase*)program, true);
    }
    return MUNIT_OK;
}

static MunitResult
test_class_statements(__attribute__((unused)) const MunitParameter params[],
                      __attribute__((unused)) void* user_data) {
    {
        const char* input = "class Foo {\n"
                            "  var a1;\n"
                            "  var a2 = 2;\n"
                            "  var a3 = 1 + 2;\n"
                            "  fn get() {\n"
                            "    return 1;\n"
                            "  };\n"
                            "  class var c = 4;\n"
                            "  class fn cget() {};\n"
                            "};";
        DaiAstProgram prog;
        DaiAstProgram_init(&prog);
        DaiAstProgram* program = &prog;
        parse_helper(input, program);
        munit_assert_int(program->length, ==, 1);
        munit_assert_int(program->statements[0]->type, ==, DaiAstType_ClassStatement);
        DaiAstClassStatement* stmt = (DaiAstClassStatement*)program->statements[0];
        munit_assert_int(stmt->type, ==, DaiAstType_ClassStatement);
        munit_assert_string_equal(stmt->name, "Foo");
        munit_assert_int(stmt->start_line, ==, 1);
        munit_assert_int(stmt->start_column, ==, 1);
        munit_assert_int(stmt->end_line, ==, 10);
        munit_assert_int(stmt->end_column, ==, 3);
        munit_assert_int(stmt->body->length, ==, 6);
        munit_assert_int(stmt->body->start_line, ==, 1);
        munit_assert_int(stmt->body->start_column, ==, 11);
        munit_assert_int(stmt->body->end_line, ==, 10);
        munit_assert_int(stmt->body->end_column, ==, 2);
        check_ins_var_statement((DaiAstInsVarStatement*)stmt->body->statements[0], "a1");
        munit_assert_null(((DaiAstInsVarStatement*)stmt->body->statements[0])->value);

        check_ins_var_statement((DaiAstInsVarStatement*)stmt->body->statements[1], "a2");
        check_integer_literal(((DaiAstInsVarStatement*)stmt->body->statements[1])->value, 2);

        check_ins_var_statement((DaiAstInsVarStatement*)stmt->body->statements[2], "a3");
        check_infix_expression_int64(
            ((DaiAstInsVarStatement*)stmt->body->statements[2])->value, 1, "+", 2);

        {
            DaiAstMethodStatement* method = (DaiAstMethodStatement*)stmt->body->statements[3];
            munit_assert_int(method->type, ==, DaiAstType_MethodStatement);
            munit_assert_string_equal(method->name, "get");
            munit_assert_int(method->parameters_count, ==, 0);
            munit_assert_int(method->body->length, ==, 1);
            munit_assert_int(method->start_line, ==, 5);
            munit_assert_int(method->start_column, ==, 3);
            munit_assert_int(method->end_line, ==, 7);
            munit_assert_int(method->end_column, ==, 5);
        }

        {
            // class var c = 4;
            check_class_var_statement((DaiAstClassVarStatement*)stmt->body->statements[4], "c");
            check_integer_literal(((DaiAstClassVarStatement*)stmt->body->statements[4])->value, 4);
        }

        {

            // class fn cget() {};
            DaiAstClassMethodStatement* method =
                (DaiAstClassMethodStatement*)stmt->body->statements[5];
            munit_assert_int(method->type, ==, DaiAstType_ClassMethodStatement);
            munit_assert_string_equal(method->name, "cget");
            munit_assert_int(method->parameters_count, ==, 0);
            munit_assert_int(method->body->length, ==, 0);
            munit_assert_int(method->start_line, ==, 9);
            munit_assert_int(method->start_column, ==, 3);
            munit_assert_int(method->end_line, ==, 9);
            munit_assert_int(method->end_column, ==, 22);
        }
        program->free_fn((DaiAstBase*)program, true);
    }

    {
        const char* input = "class Foo < Bar {\n"
                            "  var a1;\n"
                            "  var a2 = 2;\n"
                            "  var a3 = 1 + 2;\n"
                            "  fn get() {\n"
                            "    return 1;\n"
                            "  };\n"
                            "  class var c = 4;\n"
                            "  class fn cget() {};\n"
                            "};";
        DaiAstProgram prog;
        DaiAstProgram_init(&prog);
        DaiAstProgram* program = &prog;
        parse_helper(input, program);
        munit_assert_int(program->length, ==, 1);
        munit_assert_int(program->statements[0]->type, ==, DaiAstType_ClassStatement);
        DaiAstClassStatement* stmt = (DaiAstClassStatement*)program->statements[0];
        munit_assert_int(stmt->type, ==, DaiAstType_ClassStatement);
        munit_assert_string_equal(stmt->name, "Foo");
        check_identifier((DaiAstExpression*)stmt->parent, "Bar");
        munit_assert_int(stmt->start_line, ==, 1);
        munit_assert_int(stmt->start_column, ==, 1);
        munit_assert_int(stmt->end_line, ==, 10);
        munit_assert_int(stmt->end_column, ==, 3);
        munit_assert_int(stmt->body->length, ==, 6);
        check_ins_var_statement((DaiAstInsVarStatement*)stmt->body->statements[0], "a1");
        munit_assert_null(((DaiAstInsVarStatement*)stmt->body->statements[0])->value);

        check_ins_var_statement((DaiAstInsVarStatement*)stmt->body->statements[1], "a2");
        check_integer_literal(((DaiAstInsVarStatement*)stmt->body->statements[1])->value, 2);

        check_ins_var_statement((DaiAstInsVarStatement*)stmt->body->statements[2], "a3");
        check_infix_expression_int64(
            ((DaiAstInsVarStatement*)stmt->body->statements[2])->value, 1, "+", 2);

        {
            DaiAstMethodStatement* method = (DaiAstMethodStatement*)stmt->body->statements[3];
            munit_assert_int(method->type, ==, DaiAstType_MethodStatement);
            munit_assert_string_equal(method->name, "get");
            munit_assert_int(method->parameters_count, ==, 0);
            munit_assert_int(method->body->length, ==, 1);
            munit_assert_int(method->start_line, ==, 5);
            munit_assert_int(method->start_column, ==, 3);
            munit_assert_int(method->end_line, ==, 7);
            munit_assert_int(method->end_column, ==, 5);
        }

        {
            // class var c = 4;
            check_class_var_statement((DaiAstClassVarStatement*)stmt->body->statements[4], "c");
            check_integer_literal(((DaiAstClassVarStatement*)stmt->body->statements[4])->value, 4);
        }

        {

            // class fn cget() {};
            DaiAstClassMethodStatement* method =
                (DaiAstClassMethodStatement*)stmt->body->statements[5];
            munit_assert_int(method->type, ==, DaiAstType_ClassMethodStatement);
            munit_assert_string_equal(method->name, "cget");
            munit_assert_int(method->parameters_count, ==, 0);
            munit_assert_int(method->body->length, ==, 0);
            munit_assert_int(method->start_line, ==, 9);
            munit_assert_int(method->start_column, ==, 3);
            munit_assert_int(method->end_line, ==, 9);
            munit_assert_int(method->end_column, ==, 22);
        }
        program->free_fn((DaiAstBase*)program, true);
    }
    return MUNIT_OK;
}

static MunitResult
test_identifier_expression(__attribute__((unused)) const MunitParameter params[],
                           __attribute__((unused)) void* user_data) {
    const char* input = "foobar;";
    DaiAstProgram prog;
    DaiAstProgram_init(&prog);
    DaiAstProgram* program = &prog;
    parse_helper(input, program);

    munit_assert_int(program->length, ==, 1);
    DaiAstStatement* stmt = program->statements[0];
    munit_assert_int(stmt->type, ==, DaiAstType_ExpressionStatement);
    DaiAstExpression* expr = ((DaiAstExpressionStatement*)stmt)->expression;
    munit_assert_int(expr->type, ==, DaiAstType_Identifier);
    DaiAstIdentifier* ident = (DaiAstIdentifier*)expr;
    munit_assert_string_equal(ident->value, "foobar");
    {
        munit_assert_int(ident->start_line, ==, 1);
        munit_assert_int(ident->start_column, ==, 1);
        munit_assert_int(ident->end_line, ==, 1);
        munit_assert_int(ident->end_column, ==, 7);
    }

    program->free_fn((DaiAstBase*)program, true);
    return MUNIT_OK;
}

static MunitResult
test_integer_literal_expression(__attribute__((unused)) const MunitParameter params[],
                                __attribute__((unused)) void* user_data) {
    {
        const char* input = "5;";
        DaiAstProgram prog;
        DaiAstProgram_init(&prog);
        DaiAstProgram* program = &prog;
        parse_helper(input, program);

        munit_assert_int(program->length, ==, 1);
        DaiAstStatement* stmt = program->statements[0];
        munit_assert_int(stmt->type, ==, DaiAstType_ExpressionStatement);
        DaiAstExpression* expr = ((DaiAstExpressionStatement*)stmt)->expression;
        munit_assert_int(expr->type, ==, DaiAstType_IntegerLiteral);
        DaiAstIntegerLiteral* integer = (DaiAstIntegerLiteral*)expr;
        munit_assert_int(integer->value, ==, 5);
        {
            munit_assert_int(integer->start_line, ==, 1);
            munit_assert_int(integer->start_column, ==, 1);
            munit_assert_int(integer->end_line, ==, 1);
            munit_assert_int(integer->end_column, ==, 2);
        }
        program->free_fn((DaiAstBase*)program, true);
    }

    struct {
        const char* input;
        int64_t value;
        int end_column;
    } integerTests[] = {
        {"5;", 5, 2},
        {"15;", 15, 3},
        {"0b1;", 1, 4},
        {"0B1;", 1, 4},
        {"0o1;", 1, 4},
        {"0O1;", 1, 4},
        {"0x1;", 1, 4},
        {"0X1;", 1, 4},
    };

    for (int i = 0; i < sizeof(integerTests) / sizeof(integerTests[0]); i++) {
        const char* input = integerTests[i].input;
        DaiAstProgram prog;
        DaiAstProgram_init(&prog);
        DaiAstProgram* program = &prog;
        parse_helper(input, program);

        munit_assert_int(program->length, ==, 1);
        DaiAstStatement* stmt = program->statements[0];
        munit_assert_int(stmt->type, ==, DaiAstType_ExpressionStatement);
        DaiAstExpression* expr = ((DaiAstExpressionStatement*)stmt)->expression;
        munit_assert_int(expr->type, ==, DaiAstType_IntegerLiteral);
        DaiAstIntegerLiteral* integer = (DaiAstIntegerLiteral*)expr;
        munit_assert_int(integer->value, ==, integerTests[i].value);
        {
            munit_assert_int(integer->start_line, ==, 1);
            munit_assert_int(integer->start_column, ==, 1);
            munit_assert_int(integer->end_line, ==, 1);
            munit_assert_int(integer->end_column, ==, integerTests[i].end_column);
        }
        program->free_fn((DaiAstBase*)program, true);
    }
    return MUNIT_OK;
}

static MunitResult
test_float_literal_expression(__attribute__((unused)) const MunitParameter params[],
                              __attribute__((unused)) void* user_data) {
    struct {
        const char* input;
        double value;
        int end_column;
    } tests[] = {
        {"5.0;", 5, 4},
        {"15.0;", 15, 5},
        {"3.1415926;", 3.1415926, 10},
    };

    for (int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        const char* input = tests[i].input;
        DaiAstProgram prog;
        DaiAstProgram_init(&prog);
        DaiAstProgram* program = &prog;
        parse_helper(input, program);

        munit_assert_int(program->length, ==, 1);
        DaiAstStatement* stmt = program->statements[0];
        munit_assert_int(stmt->type, ==, DaiAstType_ExpressionStatement);
        DaiAstExpression* expr = ((DaiAstExpressionStatement*)stmt)->expression;
        munit_assert_int(expr->type, ==, DaiAstType_FloatLiteral);
        DaiAstFloatLiteral* num = (DaiAstFloatLiteral*)expr;
        munit_assert_double(num->value, ==, tests[i].value);
        {
            munit_assert_int(num->start_line, ==, 1);
            munit_assert_int(num->start_column, ==, 1);
            munit_assert_int(num->end_line, ==, 1);
            munit_assert_int(num->end_column, ==, tests[i].end_column);
        }
        program->free_fn((DaiAstBase*)program, true);
    }
    return MUNIT_OK;
}

static MunitResult
test_function_literal_parsing(__attribute__((unused)) const MunitParameter params[],
                              __attribute__((unused)) void* user_data) {
    const char* input = "fn(x, y) {x + y;};";
    DaiAstProgram prog;
    DaiAstProgram_init(&prog);
    DaiAstProgram* program = &prog;
    parse_helper(input, program);

    munit_assert_int(program->length, ==, 1);
    DaiAstStatement* stmt = program->statements[0];
    munit_assert_int(stmt->type, ==, DaiAstType_ExpressionStatement);
    {
        munit_assert_int(stmt->start_line, ==, 1);
        munit_assert_int(stmt->start_column, ==, 1);
        munit_assert_int(stmt->end_line, ==, 1);
        munit_assert_int(stmt->end_column, ==, 19);
    }

    DaiAstExpression* expr = ((DaiAstExpressionStatement*)stmt)->expression;
    munit_assert_int(expr->type, ==, DaiAstType_FunctionLiteral);
    DaiAstFunctionLiteral* func = (DaiAstFunctionLiteral*)expr;
    {
        munit_assert_int(func->start_line, ==, 1);
        munit_assert_int(func->start_column, ==, 1);
        munit_assert_int(func->end_line, ==, 1);
        munit_assert_int(func->end_column, ==, 18);
    }

    munit_assert_int(func->parameters_count, ==, 2);
    check_identifier((DaiAstExpression*)func->parameters[0], "x");
    check_identifier((DaiAstExpression*)func->parameters[1], "y");

    munit_assert_int(func->body->length, ==, 1);
    DaiAstExpressionStatement* body_stmt = (DaiAstExpressionStatement*)func->body->statements[0];
    munit_assert_int(body_stmt->type, ==, DaiAstType_ExpressionStatement);
    check_infix_expression_string(body_stmt->expression, "x", "+", "y");

    program->free_fn((DaiAstBase*)program, true);
    return MUNIT_OK;
}

static MunitResult
test_function_statements(__attribute__((unused)) const MunitParameter params[],
                         __attribute__((unused)) void* user_data) {
    const char* input = "fn add(x, y) {x + y;};";
    DaiAstProgram prog;
    DaiAstProgram_init(&prog);
    DaiAstProgram* program = &prog;
    parse_helper(input, program);

    munit_assert_int(program->length, ==, 1);
    DaiAstStatement* stmt = program->statements[0];
    munit_assert_int(stmt->type, ==, DaiAstType_FunctionStatement);
    {
        munit_assert_int(stmt->start_line, ==, 1);
        munit_assert_int(stmt->start_column, ==, 1);
        munit_assert_int(stmt->end_line, ==, 1);
        munit_assert_int(stmt->end_column, ==, 23);
    }

    DaiAstFunctionStatement* func = (DaiAstFunctionStatement*)stmt;
    munit_assert_string_equal(func->name, "add");
    munit_assert_int(func->parameters_count, ==, 2);
    check_identifier((DaiAstExpression*)func->parameters[0], "x");
    check_identifier((DaiAstExpression*)func->parameters[1], "y");

    munit_assert_int(func->body->length, ==, 1);
    DaiAstExpressionStatement* body_stmt = (DaiAstExpressionStatement*)func->body->statements[0];
    munit_assert_int(body_stmt->type, ==, DaiAstType_ExpressionStatement);
    check_infix_expression_string(body_stmt->expression, "x", "+", "y");

    program->free_fn((DaiAstBase*)program, true);
    return MUNIT_OK;
}

static MunitResult
test_function_parameter_parsing(__attribute__((unused)) const MunitParameter params[],
                                __attribute__((unused)) void* user_data) {
    struct {
        const char* input;
        char* expected_params[10];
        int expected_params_count;
    } tests[] = {
        {"fn() {};", {}, 0},
        {"fn(x) {};", {"x"}, 1},
        {"fn(x,) {};", {"x"}, 1},
        {"fn(x, y, z) {};", {"x", "y", "z"}, 3},
        {"fn(x, y, z,) {};", {"x", "y", "z"}, 3},
        {"fn(a,b, c, d) {};", {"a", "b", "c", "d"}, 4},
        {"fn(a,b, c, d, e2) {};", {"a", "b", "c", "d", "e2"}, 5},
    };
    for (int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        DaiAstProgram prog;
        DaiAstProgram_init(&prog);
        DaiAstProgram* program = &prog;
        parse_helper(tests[i].input, program);

        munit_assert_int(program->length, ==, 1);
        DaiAstStatement* stmt = program->statements[0];
        munit_assert_int(stmt->type, ==, DaiAstType_ExpressionStatement);
        DaiAstExpression* expr = ((DaiAstExpressionStatement*)stmt)->expression;
        munit_assert_int(expr->type, ==, DaiAstType_FunctionLiteral);
        DaiAstFunctionLiteral* func = (DaiAstFunctionLiteral*)expr;
        munit_assert_int(func->parameters_count, ==, tests[i].expected_params_count);
        for (int j = 0; j < func->parameters_count; j++) {
            check_identifier((DaiAstExpression*)func->parameters[j], tests[i].expected_params[j]);
        }
        program->free_fn((DaiAstBase*)program, true);
    }
    return MUNIT_OK;
}

static MunitResult
test_string_literal_parsing(__attribute__((unused)) const MunitParameter params[],
                            __attribute__((unused)) void* user_data) {
    struct {
        const char* input;
        const char* expected;
    } tests[] = {
        {"\"hello world\";", "\"hello world\""},
    };
    for (int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        DaiAstProgram prog;
        DaiAstProgram_init(&prog);
        DaiAstProgram* program = &prog;
        parse_helper(tests[i].input, program);
        munit_assert_int(program->length, ==, 1);
        DaiAstStatement* stmt = program->statements[0];
        munit_assert_int(stmt->type, ==, DaiAstType_ExpressionStatement);
        DaiAstExpression* expr = ((DaiAstExpressionStatement*)stmt)->expression;
        munit_assert_int(expr->type, ==, DaiAstType_StringLiteral);
        munit_assert_string_equal(((DaiAstStringLiteral*)expr)->value, tests[i].expected);
        munit_assert_int(expr->start_line, ==, 1);
        munit_assert_int(expr->start_line, ==, expr->end_line);
        munit_assert_int(expr->start_column + strlen(tests[i].expected), ==, expr->end_column);
        program->free_fn((DaiAstBase*)program, true);
    }
    return MUNIT_OK;
}
static MunitResult
test_parsing_prefix_expressions(__attribute__((unused)) const MunitParameter params[],
                                __attribute__((unused)) void* user_data) {
    struct {
        const char* input;
        const char* operator;
        int64_t integer_value;
        int end_column;
    } prefixTests[] = {
        {"!5;", "!", 5, 3},
        {"-15;", "-", 15, 4},
        {"~15;", "~", 15, 4},
    };

    for (int i = 0; i < sizeof(prefixTests) / sizeof(prefixTests[0]); i++) {
        DaiAstProgram prog;
        DaiAstProgram_init(&prog);
        DaiAstProgram* program = &prog;
        parse_helper(prefixTests[i].input, program);

        munit_assert_int(program->length, ==, 1);
        DaiAstStatement* stmt = program->statements[0];
        munit_assert_int(stmt->type, ==, DaiAstType_ExpressionStatement);
        DaiAstExpression* expr = ((DaiAstExpressionStatement*)stmt)->expression;
        munit_assert_int(expr->type, ==, DaiAstType_PrefixExpression);
        DaiAstPrefixExpression* prefix = (DaiAstPrefixExpression*)expr;
        munit_assert_string_equal(prefix->operator, prefixTests[i].operator);
        check_integer_literal(prefix->right, prefixTests[i].integer_value);
        {
            munit_assert_int(prefix->start_line, ==, 1);
            munit_assert_int(prefix->end_line, ==, 1);
            munit_assert_int(prefix->start_column, ==, 1);
            munit_assert_int(prefix->end_column, ==, prefixTests[i].end_column);
        }
        program->free_fn((DaiAstBase*)program, true);
    }

    struct {
        const char* input;
        const char* operator;
        bool value;
        int end_column;
    } boolTests[] = {
        {"!true;", "!", true, 6},
        {"!false;", "!", false, 7},
        {"not false;", "not", false, 10},
    };

    for (int i = 0; i < sizeof(boolTests) / sizeof(boolTests[0]); i++) {
        DaiAstProgram prog;
        DaiAstProgram_init(&prog);
        DaiAstProgram* program = &prog;
        parse_helper(boolTests[i].input, program);

        munit_assert_int(program->length, ==, 1);
        DaiAstStatement* stmt = program->statements[0];
        munit_assert_int(stmt->type, ==, DaiAstType_ExpressionStatement);
        DaiAstExpression* expr = ((DaiAstExpressionStatement*)stmt)->expression;
        munit_assert_int(expr->type, ==, DaiAstType_PrefixExpression);
        DaiAstPrefixExpression* prefix = (DaiAstPrefixExpression*)expr;
        munit_assert_string_equal(prefix->operator, boolTests[i].operator);
        munit_assert_int(prefix->right->type, ==, DaiAstType_Boolean);
        munit_assert_int(((DaiAstBoolean*)prefix->right)->value, ==, boolTests[i].value);
        {
            munit_assert_int(prefix->start_line, ==, 1);
            munit_assert_int(prefix->end_line, ==, 1);
            munit_assert_int(prefix->start_column, ==, 1);
            munit_assert_int(prefix->end_column, ==, boolTests[i].end_column);
        }
        program->free_fn((DaiAstBase*)program, true);
    }

    return MUNIT_OK;
}

static MunitResult
test_parsing_infix_expressions(__attribute__((unused)) const MunitParameter params[],
                               __attribute__((unused)) void* user_data) {
    struct {
        const char* input;
        int64_t left_value;
        const char* operator;
        int64_t right_value;
        int end_column;
    } infix_tests[] = {
        {"5 + 5;", 5, "+", 5, 6},
        {"5 - 5;", 5, "-", 5, 6},
        {"5 * 5;", 5, "*", 5, 6},
        {"5 / 5;", 5, "/", 5, 6},
        {"5 > 5;", 5, ">", 5, 6},
        {"5 < 5;", 5, "<", 5, 6},
        {"5 == 5;", 5, "==", 5, 7},
        {"5 != 5;", 5, "!=", 5, 7},
        {"5 % 5;", 5, "%", 5, 6},
        {"5 & 5;", 5, "&", 5, 6},
        {"5 | 5;", 5, "|", 5, 6},
        {"5 ^ 5;", 5, "^", 5, 6},
    };
    for (int i = 0; i < sizeof(infix_tests) / sizeof(infix_tests[0]); i++) {
        DaiAstProgram prog;
        DaiAstProgram_init(&prog);
        DaiAstProgram* program = &prog;
        parse_helper(infix_tests[i].input, program);

        munit_assert_int(program->length, ==, 1);
        DaiAstStatement* stmt = program->statements[0];
        munit_assert_int(stmt->type, ==, DaiAstType_ExpressionStatement);
        DaiAstExpression* expr = ((DaiAstExpressionStatement*)stmt)->expression;
        munit_assert_int(expr->type, ==, DaiAstType_InfixExpression);
        DaiAstInfixExpression* infix = (DaiAstInfixExpression*)expr;
        check_integer_literal(infix->left, infix_tests[i].left_value);
        munit_assert_string_equal(infix->operator, infix_tests[i].operator);
        check_integer_literal(infix->right, infix_tests[i].right_value);
        {
            munit_assert_int(infix->start_line, ==, 1);
            munit_assert_int(infix->start_column, ==, 1);
            munit_assert_int(infix->end_line, ==, 1);
            munit_assert_int(infix->end_column, ==, infix_tests[i].end_column);
        }
        program->free_fn((DaiAstBase*)program, true);
    }

    struct {
        const char* input;
        bool left_value;
        const char* operator;
        bool right_value;
        int end_column;
    } bool_infix_tests[] = {
        {"true == false;", true, "==", false, 14},
        {"true != false;", true, "!=", false, 14},
        {"false == false;", false, "==", false, 15},
    };
    for (int i = 0; i < sizeof(bool_infix_tests) / sizeof(bool_infix_tests[0]); i++) {
        DaiAstProgram prog;
        DaiAstProgram_init(&prog);
        DaiAstProgram* program = &prog;
        parse_helper(bool_infix_tests[i].input, program);
        munit_assert_int(program->length, ==, 1);
        DaiAstStatement* stmt = program->statements[0];
        munit_assert_int(stmt->type, ==, DaiAstType_ExpressionStatement);
        DaiAstExpression* expr = ((DaiAstExpressionStatement*)stmt)->expression;
        munit_assert_int(expr->type, ==, DaiAstType_InfixExpression);
        DaiAstInfixExpression* infix = (DaiAstInfixExpression*)expr;
        check_boolean_literal(infix->left, bool_infix_tests[i].left_value);
        munit_assert_string_equal(infix->operator, bool_infix_tests[i].operator);
        check_boolean_literal(infix->right, bool_infix_tests[i].right_value);
        {
            munit_assert_int(infix->start_line, ==, 1);
            munit_assert_int(infix->start_column, ==, 1);
            munit_assert_int(infix->end_line, ==, 1);
            munit_assert_int(infix->end_column, ==, bool_infix_tests[i].end_column);
        }
        program->free_fn((DaiAstBase*)program, true);
    }
    return MUNIT_OK;
}

static MunitResult
test_parsing_call_expressions(__attribute__((unused)) const MunitParameter params[],
                              __attribute__((unused)) void* user_data) {
    const char* input = "add(1, 2 * 3, 4 + 5);";
    DaiAstProgram prog;
    DaiAstProgram_init(&prog);
    DaiAstProgram* program = &prog;
    parse_helper(input, program);
    munit_assert_int(program->length, ==, 1);
    DaiAstStatement* stmt = program->statements[0];
    munit_assert_int(stmt->type, ==, DaiAstType_ExpressionStatement);
    {
        munit_assert_int(stmt->start_line, ==, 1);
        munit_assert_int(stmt->start_column, ==, 1);
        munit_assert_int(stmt->end_line, ==, 1);
        munit_assert_int(stmt->end_column, ==, 22);
    }
    DaiAstExpression* expr = ((DaiAstExpressionStatement*)stmt)->expression;
    munit_assert_int(expr->type, ==, DaiAstType_CallExpression);
    DaiAstCallExpression* call = (DaiAstCallExpression*)expr;
    {
        munit_assert_int(call->start_line, ==, 1);
        munit_assert_int(call->start_column, ==, 1);
        munit_assert_int(call->end_line, ==, 1);
        munit_assert_int(call->end_column, ==, 21);
    }
    check_identifier(call->function, "add");
    check_integer_literal(call->arguments[0], 1);
    check_infix_expression_int64(call->arguments[1], 2, "*", 3);
    {
        DaiAstExpression* call_arg = call->arguments[1];
        munit_assert_int(call_arg->start_line, ==, 1);
        munit_assert_int(call_arg->start_column, ==, 8);
        munit_assert_int(call_arg->end_line, ==, 1);
        munit_assert_int(call_arg->end_column, ==, 13);
    }
    check_infix_expression_int64(call->arguments[2], 4, "+", 5);
    {
        DaiAstExpression* call_arg = call->arguments[2];
        munit_assert_int(call_arg->start_line, ==, 1);
        munit_assert_int(call_arg->start_column, ==, 15);
        munit_assert_int(call_arg->end_line, ==, 1);
        munit_assert_int(call_arg->end_column, ==, 20);
    }

    program->free_fn((DaiAstBase*)program, true);
    return MUNIT_OK;
}

static MunitResult
test_parsing_call_expression_parameter_parsing(__attribute__((unused))
                                               const MunitParameter params[],
                                               __attribute__((unused)) void* user_data) {
    struct {
        const char* input;
        const char* expected_indent;
        const char* expected_args[10];
        int expected_args_count;
    } tests[] = {
        {"add();", "add", {""}, 0},
        {"add(1);", "add", {"1"}, 1},
        {"add(1,);", "add", {"1"}, 1},
        {"add(1, 2);", "add", {"1", "2"}, 2},
        {"add(1, 2,);", "add", {"1", "2"}, 2},
        {"add(1, 2 * 3, 4 + 5);", "add", {"1", "(2 * 3)", "(4 + 5)"}, 3},
    };
    for (int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        DaiAstProgram prog;
        DaiAstProgram_init(&prog);
        DaiAstProgram* program = &prog;
        parse_helper(tests[i].input, program);
        munit_assert_int(program->length, ==, 1);
        DaiAstStatement* stmt = program->statements[0];
        munit_assert_int(stmt->type, ==, DaiAstType_ExpressionStatement);
        DaiAstExpression* expr = ((DaiAstExpressionStatement*)stmt)->expression;
        munit_assert_int(expr->type, ==, DaiAstType_CallExpression);
        DaiAstCallExpression* call = (DaiAstCallExpression*)expr;
        munit_assert_int(call->arguments_count, ==, tests[i].expected_args_count);
        check_identifier(call->function, tests[i].expected_indent);
        for (int j = 0; j < tests[i].expected_args_count; j++) {
            DaiAstExpression* arg = call->arguments[j];
            char* s               = arg->literal_fn(arg);
            munit_assert_string_equal(s, tests[i].expected_args[j]);
            dai_free(s);
        }
        program->free_fn((DaiAstBase*)program, true);
    }
    return MUNIT_OK;
}

static MunitResult
test_operator_precedence_parsing(__attribute__((unused)) const MunitParameter params[],
                                 __attribute__((unused)) void* user_data) {
    struct {
        const char* input;
        const char* expected;
    } tests[] = {
        {
            "-a * b;",
            "((- a) * b)",
        },
        {
            "!-a;",
            "(! (- a))",
        },
        {
            "a + b + c;",
            "((a + b) + c)",
        },
        {
            "a + b - c;",
            "((a + b) - c)",
        },
        {
            "a * b * c;",
            "((a * b) * c)",
        },
        {
            "a * b / c;",
            "((a * b) / c)",
        },
        {
            "a + b / c;",
            "(a + (b / c))",
        },
        {
            "a + b * c + d / e - f;",
            "(((a + (b * c)) + (d / e)) - f)",
        },
        {
            "5 > 4 == 3 < 4;",
            "(((5 > 4) == 3) < 4)",
        },
        {
            "5 < 4 != 3 > 4;",
            "(((5 < 4) != 3) > 4)",
        },
        {
            "3 + 4 * 5 == 3 * 1 + 4 * 5;",
            "((3 + (4 * 5)) == ((3 * 1) + (4 * 5)))",
        },
        {
            "true;",
            "true",
        },
        {
            "false;",
            "false",
        },
        {
            "3 > 5 == false;",
            "((3 > 5) == false)",
        },
        {
            "3 >= 5;",
            "(3 >= 5)",
        },
        {
            "3 >= 5 == false;",
            "((3 >= 5) == false)",
        },
        {
            "3 < 5 == true;",
            "((3 < 5) == true)",
        },
        {
            "3 <= 5 == true;",
            "((3 <= 5) == true)",
        },
        {
            "1 + (2 + 3) + 4;",
            "((1 + (2 + 3)) + 4)",
        },
        {
            "(5 + 5) * 2;",
            "((5 + 5) * 2)",
        },
        {
            "2 / (5 + 5);",
            "(2 / (5 + 5))",
        },
        {
            "(5 + 5) * 2 * (5 + 5);",
            "(((5 + 5) * 2) * (5 + 5))",
        },
        {
            "-(5 + 5);",
            "(- (5 + 5))",
        },
        {
            "!(true == true);",
            "(! (true == true))",
        },
        {
            "a + add(b * c) + d;",
            "((a + add((b * c))) + d)",
        },
        {
            "add(a, b, 1, 2 * 3, 4 + 5, add(6, 7 * 8));",
            "add(a, b, 1, (2 * 3), (4 + 5), add(6, (7 * 8)))",
        },
        {
            "add(a + b + c * d / f + g);",
            "add((((a + b) + ((c * d) / f)) + g))",
        },
        {
            "self;",
            "(self)",
        },
        {
            "self.a * 3 + super.b;",
            "(((self.a) * 3) + (super.b))",
        },
        {
            "5 + 5 % 2;",
            "(5 + (5 % 2))",
        },
        {
            "(5 + 5) % 2;",
            "((5 + 5) % 2)",
        },
        {
            "5 * 5 / 2 % 2;",
            "(((5 * 5) / 2) % 2)",
        },
        {
            "1 == 1 and 2 == 2;",
            "((1 == 1) and (2 == 2))",
        },
        {
            "1 == 1 or 2 == 2;",
            "((1 == 1) or (2 == 2))",
        },
        {
            "true and false or true;",
            "((true and false) or true)",
        },
        {
            "not true;",
            "(not true)",
        },
        {
            "1 << 2 - 1 < 1 >> 2 - 1;",
            "((1 << (2 - 1)) < (1 >> (2 - 1)))",
        },
        {
            "1 << 2 - 1 & 1 < 1 >> 2 - 1 | 1;",
            "(((1 << (2 - 1)) & 1) < ((1 >> (2 - 1)) | 1))",
        },
        {
            "1 | 2 << 1;",
            "(1 | (2 << 1))",
        },
        {
            "~12 + 4;",
            "((~ 12) + 4)",
        },
        {
            "1 | 2 ^ 3 & 4;",
            "(1 | (2 ^ (3 & 4)))",
        },
        {
            "[1];",
            "[1, ]",
        },
        {
            "x[1] * 2;",
            "((x[1]) * 2)",
        },
        {
            "x[1](2);",
            "(x[1])(2)",
        },
        {
            "x(2)[1];",
            "(x(2)[1])",
        },
    };
    for (int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        DaiAstProgram prog;
        DaiAstProgram_init(&prog);
        DaiAstProgram* program = &prog;
        parse_helper(tests[i].input, program);

        munit_assert_int(program->length, ==, 1);
        DaiAstExpressionStatement* stmt = (DaiAstExpressionStatement*)program->statements[0];
        munit_assert_int(stmt->type, ==, DaiAstType_ExpressionStatement);
        DaiAstExpression* expr = stmt->expression;
        char* literal          = expr->literal_fn(expr);
        munit_assert_string_equal(literal, tests[i].expected);
        dai_free(literal);

        program->free_fn((DaiAstBase*)program, true);
    }
    return MUNIT_OK;
}

static MunitResult
test_boolean_expression(__attribute__((unused)) const MunitParameter params[],
                        __attribute__((unused)) void* user_data) {
    struct {
        const char* input;
        bool expected;
    } tests[] = {
        {
            "true;",
            true,
        },
        {
            "false;",
            false,
        },
    };
    for (int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        DaiAstProgram prog;
        DaiAstProgram_init(&prog);
        DaiAstProgram* program = &prog;
        parse_helper(tests[i].input, program);

        munit_assert_int(program->length, ==, 1);
        DaiAstExpressionStatement* stmt = (DaiAstExpressionStatement*)program->statements[0];
        munit_assert_int(stmt->type, ==, DaiAstType_ExpressionStatement);
        DaiAstExpression* expr = stmt->expression;
        munit_assert_int(expr->type, ==, DaiAstType_Boolean);
        munit_assert_int(((DaiAstBoolean*)expr)->value, ==, tests[i].expected);
        program->free_fn((DaiAstBase*)program, true);
    }
    return MUNIT_OK;
}

static MunitResult
test_nil_expression(__attribute__((unused)) const MunitParameter params[],
                    __attribute__((unused)) void* user_data) {
    struct {
        const char* input;
        bool expected;
    } tests[] = {
        {
            "nil;",
            true,
        },
    };
    for (int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        DaiAstProgram prog;
        DaiAstProgram_init(&prog);
        DaiAstProgram* program = &prog;
        parse_helper(tests[i].input, program);

        munit_assert_int(program->length, ==, 1);
        DaiAstExpressionStatement* stmt = (DaiAstExpressionStatement*)program->statements[0];
        munit_assert_int(stmt->type, ==, DaiAstType_ExpressionStatement);
        DaiAstExpression* expr = stmt->expression;
        munit_assert_int(expr->type, ==, DaiAstType_Nil);
        program->free_fn((DaiAstBase*)program, true);
    }
    return MUNIT_OK;
}

static MunitResult
test_syntax_error(__attribute__((unused)) const MunitParameter params[],
                  __attribute__((unused)) void* user_data) {
    struct {
        const char* input;
        const char* expected;
    } tests[] = {
        {
            "let a = 5;",
            "SyntaxError: expected token to be \"DaiTokenType_semicolon\" but "
            "got \"DaiTokenType_ident\" in "
            "<test>:1:5",
        },
        {
            "var var = 5;",
            "SyntaxError: expected token to be \"DaiTokenType_ident\" but got "
            "\"DaiTokenType_var\" in "
            "<test>:1:5",
        },
        {
            "var a a 5;",
            "SyntaxError: expected token to be \"DaiTokenType_assign\" but got "
            "\"DaiTokenType_ident\" in "
            "<test>:1:7",
        },
        {
            "* 3 + 5;",
            "SyntaxError: no prefix parse function for \"DaiTokenType_asterisk\" "
            "found in <test>:1:1",
        },
        {
            "0b12;",
            "SyntaxError: expected token to be \"DaiTokenType_semicolon\" but got "
            "\"DaiTokenType_int\" in <test>:1:4",
        },
        {
            "0B12;",
            "SyntaxError: expected token to be \"DaiTokenType_semicolon\" but got "
            "\"DaiTokenType_int\" in <test>:1:4",
        },
        {
            "0o18;",
            "SyntaxError: expected token to be \"DaiTokenType_semicolon\" but got "
            "\"DaiTokenType_int\" in <test>:1:4",
        },
        {
            "0O18;",
            "SyntaxError: expected token to be \"DaiTokenType_semicolon\" but got "
            "\"DaiTokenType_int\" in <test>:1:4",
        },
        {
            "0x1g;",
            "SyntaxError: expected token to be \"DaiTokenType_semicolon\" but got "
            "\"DaiTokenType_ident\" in <test>:1:4",
        },
        {
            "0X1g;",
            "SyntaxError: expected token to be \"DaiTokenType_semicolon\" but got "
            "\"DaiTokenType_ident\" in <test>:1:4",
        },
        {
            "1 + 2",
            "SyntaxError: expected token to be \"DaiTokenType_semicolon\" but "
            "got \"DaiTokenType_eof\" in <test>:1:6",
        },
        {
            "var a =;",
            "SyntaxError: no prefix parse function for "
            "\"DaiTokenType_semicolon\" found in <test>:1:8",
        },
        {
            "var a = 1",
            "SyntaxError: expected token to be \"DaiTokenType_semicolon\" but "
            "got \"DaiTokenType_eof\" in <test>:1:10",
        },
        {
            "a(,)",
            "SyntaxError: no prefix parse function for \"DaiTokenType_comma\" "
            "found in <test>:1:3",
        },
        {
            "a(1,* 2)",
            "SyntaxError: no prefix parse function for \"DaiTokenType_asterisk\" "
            "found in <test>:1:5",
        },
        {
            "a(1, 2;",
            "SyntaxError: expected token to be \"DaiTokenType_rparen\" but got "
            "\"DaiTokenType_semicolon\" in <test>:1:7",
        },
        {
            "if (a) { var b;}",
            "SyntaxError: expected token to be \"DaiTokenType_assign\" but got "
            "\"DaiTokenType_semicolon\" in <test>:1:15",
        },
        {
            "if (a) { var b = 1;",
            "SyntaxError: expected '}' in <test>:1:20",
        },
        {
            "if a) { var b = 1;",
            "SyntaxError: expected token to be \"DaiTokenType_lparen\" but got "
            "\"DaiTokenType_ident\" in <test>:1:4",
        },
        {
            "if () { var b = 1;",
            "SyntaxError: no prefix parse function for \"DaiTokenType_rparen\" "
            "found in <test>:1:5",
        },
        {
            "if (a { var b = 1;",
            "SyntaxError: expected token to be \"DaiTokenType_rparen\" but got "
            "\"DaiTokenType_lbrace\" in <test>:1:7",
        },
        {
            "if (a) var b = 1;}",
            "SyntaxError: expected token to be \"DaiTokenType_lbrace\" but got "
            "\"DaiTokenType_var\" in <test>:1:8",
        },
        {
            "if (a) {} else var b = 1;}",
            "SyntaxError: expected token to be \"DaiTokenType_lbrace\" but got "
            "\"DaiTokenType_var\" in <test>:1:16",
        },
        {
            "if (a) {} else {var b1;}",
            "SyntaxError: expected token to be \"DaiTokenType_assign\" but got "
            "\"DaiTokenType_semicolon\" in <test>:1:23",
        },
        {
            "var foo = fn(1) { 1; };",
            "SyntaxError: expected token to be \"DaiTokenType_ident\" but got "
            "\"DaiTokenType_int\" in <test>:1:14",
        },
        {
            "var foo = fn(a { 1; };",
            "SyntaxError: expected token to be \"DaiTokenType_rparen\" but got "
            "\"DaiTokenType_lbrace\" in <test>:1:16",
        },
        {
            "var foo = fn(a,1) { 1; };",
            "SyntaxError: expected token to be \"DaiTokenType_ident\" but got "
            "\"DaiTokenType_int\" in <test>:1:16",
        },
        {
            "var foo = fn a,) { 1; };",
            "SyntaxError: expected token to be \"DaiTokenType_lparen\" but got "
            "\"DaiTokenType_ident\" in <test>:1:14",
        },
        {
            "var foo = fn (a,) { var; };",
            "SyntaxError: expected token to be \"DaiTokenType_ident\" but got "
            "\"DaiTokenType_semicolon\" in <test>:1:24",
        },
        {
            "var foo = fn (a,)   };",
            "SyntaxError: expected token to be \"DaiTokenType_lbrace\" but got "
            "\"DaiTokenType_rbrace\" in <test>:1:21",
        },
        {
            "(a;",
            "SyntaxError: expected token to be \"DaiTokenType_rparen\" but got "
            "\"DaiTokenType_semicolon\" in <test>:1:3",
        },
        {
            "-;",
            "SyntaxError: no prefix parse function for "
            "\"DaiTokenType_semicolon\" found in <test>:1:2",
        },
        {
            "1-;",
            "SyntaxError: no prefix parse function for "
            "\"DaiTokenType_semicolon\" found in <test>:1:3",
        },
        {
            "return 1 - ;",
            "SyntaxError: no prefix parse function for "
            "\"DaiTokenType_semicolon\" found in <test>:1:12",
        },
        {
            "return 1",
            "SyntaxError: expected token to be \"DaiTokenType_semicolon\" but "
            "got \"DaiTokenType_eof\" in <test>:1:9",
        },
        {
            "a = ",
            "SyntaxError: no prefix parse function for \"DaiTokenType_eof\" "
            "found in <test>:1:5",
        },
        {
            "a = -",
            "SyntaxError: no prefix parse function for \"DaiTokenType_eof\" "
            "found in <test>:1:6",
        },
        {
            "a = 3 + 4 * 5",
            "SyntaxError: expected token to be \"DaiTokenType_semicolon\" but "
            "got "
            "\"DaiTokenType_eof\" in <test>:1:14",
        },
        {
            "this.",
            "SyntaxError: expected token to be \"DaiTokenType_ident\" but got "
            "\"DaiTokenType_eof\" "
            "in <test>:1:6",
        },
        {
            "a.123",
            "SyntaxError: expected token to be \"DaiTokenType_ident\" but got "
            "\"DaiTokenType_int\" "
            "in <test>:1:3",
        },
        {
            "super.a.",
            "SyntaxError: expected token to be \"DaiTokenType_ident\" but got "
            "\"DaiTokenType_eof\" "
            "in <test>:1:9",
        },
        {
            "this.a = ",
            "SyntaxError: no prefix parse function for \"DaiTokenType_eof\" "
            "found in <test>:1:10",
        },
    };
    for (int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        DaiSyntaxError* syntax_error = parse_error(tests[i].input);
        munit_assert_not_null(syntax_error);
        char* error_message = DaiSyntaxError_string(syntax_error);
        munit_assert_string_equal(error_message, tests[i].expected);
        free(error_message);
        DaiSyntaxError_free(syntax_error);
    }
    return MUNIT_OK;
}

static void
recursive_string_and_free(DaiAstBase* ast) {
    switch (ast->type) {
        case DaiAstType_program: {
            DaiAstProgram* program = (DaiAstProgram*)ast;
            char* s                = program->string_fn((DaiAstBase*)program, false);
            free(s);
            for (size_t i = 0; i < program->length; i++) {
                recursive_string_and_free((DaiAstBase*)program->statements[i]);
            }
            program->free_fn((DaiAstBase*)program, false);
            break;
        }
        case DaiAstType_VarStatement: {
            DaiAstVarStatement* statement = (DaiAstVarStatement*)ast;
            char* s                       = statement->string_fn((DaiAstBase*)statement, false);
            free(s);
            recursive_string_and_free((DaiAstBase*)statement->name);
            recursive_string_and_free((DaiAstBase*)statement->value);
            statement->free_fn((DaiAstBase*)statement, false);
            break;
        }
        case DaiAstType_ReturnStatement: {
            DaiAstReturnStatement* statement = (DaiAstReturnStatement*)ast;
            char* s                          = statement->string_fn((DaiAstBase*)statement, false);
            free(s);
            recursive_string_and_free((DaiAstBase*)statement->return_value);
            statement->free_fn((DaiAstBase*)statement, false);
            break;
        }
        case DaiAstType_ExpressionStatement: {
            DaiAstExpressionStatement* statement = (DaiAstExpressionStatement*)ast;
            char* s = statement->string_fn((DaiAstBase*)statement, false);
            free(s);
            recursive_string_and_free((DaiAstBase*)statement->expression);
            statement->free_fn((DaiAstBase*)statement, false);
            break;
        }
        case DaiAstType_IfStatement: {
            DaiAstIfStatement* statement = (DaiAstIfStatement*)ast;
            char* s                      = statement->string_fn((DaiAstBase*)statement, false);
            free(s);
            recursive_string_and_free((DaiAstBase*)statement->condition);
            recursive_string_and_free((DaiAstBase*)statement->then_branch);
            for (int i = 0; i < statement->elif_branch_count; i++) {
                recursive_string_and_free((DaiAstBase*)statement->elif_branches[i].condition);
                recursive_string_and_free((DaiAstBase*)statement->elif_branches[i].then_branch);
            }
            if (statement->else_branch) {
                recursive_string_and_free((DaiAstBase*)statement->else_branch);
            }
            statement->free_fn((DaiAstBase*)statement, false);
            break;
        }
        case DaiAstType_BlockStatement: {
            DaiAstBlockStatement* statement = (DaiAstBlockStatement*)ast;
            char* s                         = statement->string_fn((DaiAstBase*)statement, false);
            free(s);
            for (size_t i = 0; i < statement->length; i++) {
                recursive_string_and_free((DaiAstBase*)statement->statements[i]);
            }
            statement->free_fn((DaiAstBase*)statement, false);
            break;
        }
        case DaiAstType_AssignStatement: {
            DaiAstAssignStatement* statement = (DaiAstAssignStatement*)ast;
            {
                char* s = statement->string_fn((DaiAstBase*)statement, false);
                free(s);
            }
            recursive_string_and_free((DaiAstBase*)statement->left);
            recursive_string_and_free((DaiAstBase*)statement->value);
            statement->free_fn((DaiAstBase*)statement, false);
            break;
        }
        case DaiAstType_FunctionStatement: {
            DaiAstFunctionStatement* statement = (DaiAstFunctionStatement*)ast;
            char* s = statement->string_fn((DaiAstBase*)statement, false);
            free(s);
            for (size_t i = 0; i < statement->parameters_count; i++) {
                recursive_string_and_free((DaiAstBase*)statement->parameters[i]);
            }
            recursive_string_and_free((DaiAstBase*)statement->body);
            statement->free_fn((DaiAstBase*)statement, false);
            break;
        }
        case DaiAstType_ClassStatement: {
            DaiAstClassStatement* statement = (DaiAstClassStatement*)ast;
            char* s                         = statement->string_fn((DaiAstBase*)statement, false);
            free(s);
            if (statement->parent) {
                recursive_string_and_free((DaiAstBase*)statement->parent);
            }
            recursive_string_and_free((DaiAstBase*)statement->body);
            statement->free_fn((DaiAstBase*)statement, false);
            break;
        }
        case DaiAstType_InsVarStatement: {
            DaiAstInsVarStatement* statement = (DaiAstInsVarStatement*)ast;
            char* s                          = statement->string_fn((DaiAstBase*)statement, false);
            free(s);
            recursive_string_and_free((DaiAstBase*)statement->name);
            if (statement->value) {
                recursive_string_and_free((DaiAstBase*)statement->value);
            }
            statement->free_fn((DaiAstBase*)statement, false);
            break;
        }
        case DaiAstType_MethodStatement: {
            DaiAstMethodStatement* statement = (DaiAstMethodStatement*)ast;
            char* s                          = statement->string_fn((DaiAstBase*)statement, false);
            free(s);
            for (size_t i = 0; i < statement->parameters_count; i++) {
                recursive_string_and_free((DaiAstBase*)statement->parameters[i]);
            }
            recursive_string_and_free((DaiAstBase*)statement->body);
            statement->free_fn((DaiAstBase*)statement, false);
            break;
        }
        case DaiAstType_ClassVarStatement: {
            DaiAstClassVarStatement* statement = (DaiAstClassVarStatement*)ast;
            char* s = statement->string_fn((DaiAstBase*)statement, false);
            free(s);
            recursive_string_and_free((DaiAstBase*)statement->name);
            recursive_string_and_free((DaiAstBase*)statement->value);
            statement->free_fn((DaiAstBase*)statement, false);
            break;
        }
        case DaiAstType_ClassMethodStatement: {
            DaiAstClassMethodStatement* statement = (DaiAstClassMethodStatement*)ast;
            char* s = statement->string_fn((DaiAstBase*)statement, false);
            free(s);
            for (size_t i = 0; i < statement->parameters_count; i++) {
                recursive_string_and_free((DaiAstBase*)statement->parameters[i]);
            }
            recursive_string_and_free((DaiAstBase*)statement->body);
            statement->free_fn((DaiAstBase*)statement, false);
            break;
        }
        case DaiAstType_BreakStatement: {
            DaiAstBreakStatement* statement = (DaiAstBreakStatement*)ast;
            char* s                         = statement->string_fn((DaiAstBase*)statement, false);
            free(s);
            statement->free_fn((DaiAstBase*)statement, false);
            break;
        }
        case DaiAstType_ContinueStatement: {
            DaiAstContinueStatement* statement = (DaiAstContinueStatement*)ast;
            char* s = statement->string_fn((DaiAstBase*)statement, false);
            free(s);
            statement->free_fn((DaiAstBase*)statement, false);
            break;
        }
        case DaiAstType_WhileStatement: {
            DaiAstWhileStatement* stmt = (DaiAstWhileStatement*)ast;
            char* s                    = stmt->string_fn((DaiAstBase*)stmt, false);
            free(s);
            recursive_string_and_free((DaiAstBase*)stmt->condition);
            recursive_string_and_free((DaiAstBase*)stmt->body);
            stmt->free_fn((DaiAstBase*)stmt, false);
            break;
        }
        case DaiAstType_IntegerLiteral: {
            DaiAstIntegerLiteral* literal = (DaiAstIntegerLiteral*)ast;
            char* s                       = literal->string_fn((DaiAstBase*)literal, false);
            free(s);
            literal->free_fn((DaiAstBase*)literal, false);
            break;
        }
        case DaiAstType_FloatLiteral: {
            DaiAstFloatLiteral* literal = (DaiAstFloatLiteral*)ast;
            char* s                     = literal->string_fn((DaiAstBase*)literal, false);
            free(s);
            literal->free_fn((DaiAstBase*)literal, false);
            break;
        }
        case DaiAstType_Boolean: {
            DaiAstBoolean* literal = (DaiAstBoolean*)ast;
            char* s                = literal->string_fn((DaiAstBase*)literal, false);
            free(s);
            literal->free_fn((DaiAstBase*)literal, false);
            break;
        }
        case DaiAstType_Nil: {
            DaiAstNil* literal = (DaiAstNil*)ast;
            char* s            = literal->string_fn((DaiAstBase*)literal, false);
            free(s);
            literal->free_fn((DaiAstBase*)literal, false);
            break;
        }
        case DaiAstType_Identifier: {
            DaiAstIdentifier* identifier = (DaiAstIdentifier*)ast;
            char* s                      = identifier->string_fn((DaiAstBase*)identifier, false);
            free(s);
            identifier->free_fn((DaiAstBase*)identifier, false);
            break;
        }
        case DaiAstType_PrefixExpression: {
            DaiAstPrefixExpression* expression = (DaiAstPrefixExpression*)ast;
            char* s = expression->string_fn((DaiAstBase*)expression, false);
            free(s);
            recursive_string_and_free((DaiAstBase*)expression->right);
            expression->free_fn((DaiAstBase*)expression, false);
            break;
        }
        case DaiAstType_InfixExpression: {
            DaiAstInfixExpression* expression = (DaiAstInfixExpression*)ast;
            char* s = expression->string_fn((DaiAstBase*)expression, false);
            free(s);
            recursive_string_and_free((DaiAstBase*)expression->left);
            recursive_string_and_free((DaiAstBase*)expression->right);
            expression->free_fn((DaiAstBase*)expression, false);
            break;
        }
        case DaiAstType_FunctionLiteral: {
            DaiAstFunctionLiteral* literal = (DaiAstFunctionLiteral*)ast;
            {
                char* s = literal->string_fn((DaiAstBase*)literal, false);
                free(s);
            }
            {
                char* s = literal->literal_fn((DaiAstExpression*)literal);
                free(s);
            }
            for (size_t i = 0; i < literal->parameters_count; i++) {
                recursive_string_and_free((DaiAstBase*)literal->parameters[i]);
            }
            recursive_string_and_free((DaiAstBase*)literal->body);
            literal->free_fn((DaiAstBase*)literal, false);
            break;
        }
        case DaiAstType_CallExpression: {
            DaiAstCallExpression* expression = (DaiAstCallExpression*)ast;
            char* s = expression->string_fn((DaiAstBase*)expression, false);
            free(s);
            recursive_string_and_free((DaiAstBase*)expression->function);
            for (size_t i = 0; i < expression->arguments_count; i++) {
                recursive_string_and_free((DaiAstBase*)expression->arguments[i]);
            }
            expression->free_fn((DaiAstBase*)expression, false);
            break;
        }
        case DaiAstType_DotExpression: {
            DaiAstDotExpression* expression = (DaiAstDotExpression*)ast;
            {
                char* s = expression->string_fn((DaiAstBase*)expression, false);
                free(s);
            }
            {
                char* s = expression->literal_fn((DaiAstExpression*)expression);
                free(s);
            }
            recursive_string_and_free((DaiAstBase*)expression->left);
            recursive_string_and_free((DaiAstBase*)expression->name);
            expression->free_fn((DaiAstBase*)expression, false);
            break;
        }
        case DaiAstType_SelfExpression: {
            DaiAstSelfExpression* expression = (DaiAstSelfExpression*)ast;
            {
                char* s = expression->string_fn((DaiAstBase*)expression, false);
                free(s);
            }
            {
                char* s = expression->literal_fn((DaiAstExpression*)expression);
                free(s);
            }
            if (expression->name != NULL) {
                recursive_string_and_free((DaiAstBase*)expression->name);
            }
            expression->free_fn((DaiAstBase*)expression, false);
            break;
        }
        case DaiAstType_SuperExpression: {
            DaiAstSuperExpression* expression = (DaiAstSuperExpression*)ast;
            {
                char* s = expression->string_fn((DaiAstBase*)expression, false);
                free(s);
            }
            {
                char* s = expression->literal_fn((DaiAstExpression*)expression);
                free(s);
            }
            if (expression->name != NULL) {
                recursive_string_and_free((DaiAstBase*)expression->name);
            }
            expression->free_fn((DaiAstBase*)expression, false);
            break;
        }
        case DaiAstType_StringLiteral: {
            DaiAstStringLiteral* literal = (DaiAstStringLiteral*)ast;
            char* s                      = literal->string_fn((DaiAstBase*)literal, false);
            free(s);
            s = literal->literal_fn((DaiAstExpression*)literal);
            free(s);
            literal->free_fn((DaiAstBase*)literal, false);
            break;
        }
        case DaiAstType_ArrayLiteral: {
            DaiAstArrayLiteral* literal = (DaiAstArrayLiteral*)ast;
            char* s                     = literal->string_fn((DaiAstBase*)literal, false);
            free(s);
            s = literal->literal_fn((DaiAstExpression*)literal);
            munit_assert_string_equal(s, "[1, 3.2, \"abc\", ]");
            free(s);
            literal->free_fn((DaiAstBase*)literal, true);
            break;
        }
        case DaiAstType_SubscriptExpression: {
            DaiAstSubscriptExpression* expression = (DaiAstSubscriptExpression*)ast;
            char* s = expression->string_fn((DaiAstBase*)expression, false);
            free(s);
            s = expression->literal_fn((DaiAstExpression*)expression);
            free(s);
            recursive_string_and_free((DaiAstBase*)expression->left);
            recursive_string_and_free((DaiAstBase*)expression->right);
            expression->free_fn((DaiAstBase*)expression, false);
            break;
        }
        case DaiAstType_MapLiteral: {
            DaiAstMapLiteral* literal = (DaiAstMapLiteral*)ast;
            char* s                   = literal->string_fn((DaiAstBase*)literal, false);
            free(s);
            s = literal->literal_fn((DaiAstExpression*)literal);
            munit_assert_string_equal(s, "{1: 2, 2: 3, }");
            free(s);
            for (size_t i = 0; i < literal->length; i++) {
                recursive_string_and_free((DaiAstBase*)literal->pairs[i].key);
                recursive_string_and_free((DaiAstBase*)literal->pairs[i].value);
            }
            literal->free_fn((DaiAstBase*)literal, false);
            break;
        }
        case DaiAstType_ForInStatement: {
            DaiAstForInStatement* statement = (DaiAstForInStatement*)ast;
            char* s                         = statement->string_fn((DaiAstBase*)statement, false);
            free(s);
            recursive_string_and_free((DaiAstBase*)statement->i);
            recursive_string_and_free((DaiAstBase*)statement->e);
            recursive_string_and_free((DaiAstBase*)statement->expression);
            recursive_string_and_free((DaiAstBase*)statement->body);
            statement->free_fn((DaiAstBase*)statement, false);
            break;
        }

        default: {
            unreachable();
            break;
        }
    }
}

static MunitResult
test_parse_example(__attribute__((unused)) const MunitParameter params[],
                   __attribute__((unused)) void* user_data) {
    char resolved_path[PATH_MAX];
    get_file_directory(resolved_path);
    strcat(resolved_path, "parse_example.dai");
    char* input = dai_string_from_file(resolved_path);
    munit_assert_not_null(input);

    DaiAstProgram prog;
    DaiAstProgram_init(&prog);
    DaiAstProgram* program = &prog;
    parse_helper(input, program);

    {
#ifdef DAI_SANTEST_OUTPUT
        char* s = program->string_fn((DaiAstBase*)program, true);
        printf("%s\n", s);
        free(s);
#endif
    }
    recursive_string_and_free((DaiAstBase*)program);
    free(input);
    return MUNIT_OK;
}

static MunitResult
test_array_literal_expression(__attribute__((unused)) const MunitParameter params[],
                              __attribute__((unused)) void* user_data) {
    {
        const char* input = "[];";
        DaiAstProgram prog;
        DaiAstProgram_init(&prog);
        DaiAstProgram* program = &prog;
        parse_helper(input, program);

        munit_assert_int(program->length, ==, 1);
        DaiAstStatement* stmt = program->statements[0];
        munit_assert_int(stmt->type, ==, DaiAstType_ExpressionStatement);
        DaiAstExpression* expr = ((DaiAstExpressionStatement*)stmt)->expression;
        munit_assert_int(expr->type, ==, DaiAstType_ArrayLiteral);
        const DaiAstArrayLiteral* array = (DaiAstArrayLiteral*)expr;
        munit_assert_size(array->length, ==, 0);
        {
            munit_assert_int(array->start_line, ==, 1);
            munit_assert_int(array->start_column, ==, 1);
            munit_assert_int(array->end_line, ==, 1);
            munit_assert_int(array->end_column, ==, 3);
        }
        program->free_fn((DaiAstBase*)program, true);
    }
    {
        const char* input = "[1];";
        DaiAstProgram prog;
        DaiAstProgram_init(&prog);
        DaiAstProgram* program = &prog;
        parse_helper(input, program);

        munit_assert_int(program->length, ==, 1);
        DaiAstStatement* stmt = program->statements[0];
        munit_assert_int(stmt->type, ==, DaiAstType_ExpressionStatement);
        DaiAstExpression* expr = ((DaiAstExpressionStatement*)stmt)->expression;
        munit_assert_int(expr->type, ==, DaiAstType_ArrayLiteral);
        const DaiAstArrayLiteral* array = (DaiAstArrayLiteral*)expr;
        munit_assert_size(array->length, ==, 1);
        check_integer_literal(array->elements[0], 1);
        {
            munit_assert_int(array->start_line, ==, 1);
            munit_assert_int(array->start_column, ==, 1);
            munit_assert_int(array->end_line, ==, 1);
            munit_assert_int(array->end_column, ==, 4);
        }
        program->free_fn((DaiAstBase*)program, true);
    }
    {
        const char* input = "[1,];";
        DaiAstProgram prog;
        DaiAstProgram_init(&prog);
        DaiAstProgram* program = &prog;
        parse_helper(input, program);

        munit_assert_int(program->length, ==, 1);
        DaiAstStatement* stmt = program->statements[0];
        munit_assert_int(stmt->type, ==, DaiAstType_ExpressionStatement);
        DaiAstExpression* expr = ((DaiAstExpressionStatement*)stmt)->expression;
        munit_assert_int(expr->type, ==, DaiAstType_ArrayLiteral);
        const DaiAstArrayLiteral* array = (DaiAstArrayLiteral*)expr;
        munit_assert_size(array->length, ==, 1);
        check_integer_literal(array->elements[0], 1);
        {
            munit_assert_int(array->start_line, ==, 1);
            munit_assert_int(array->start_column, ==, 1);
            munit_assert_int(array->end_line, ==, 1);
            munit_assert_int(array->end_column, ==, 5);
        }
        program->free_fn((DaiAstBase*)program, true);
    }
    {
        const char* input = "[1, 2 * 2];";
        DaiAstProgram prog;
        DaiAstProgram_init(&prog);
        DaiAstProgram* program = &prog;
        parse_helper(input, program);

        munit_assert_int(program->length, ==, 1);
        DaiAstStatement* stmt = program->statements[0];
        munit_assert_int(stmt->type, ==, DaiAstType_ExpressionStatement);
        DaiAstExpression* expr = ((DaiAstExpressionStatement*)stmt)->expression;
        munit_assert_int(expr->type, ==, DaiAstType_ArrayLiteral);
        const DaiAstArrayLiteral* array = (DaiAstArrayLiteral*)expr;
        munit_assert_size(array->length, ==, 2);
        check_integer_literal(array->elements[0], 1);
        check_infix_expression_int64(array->elements[1], 2, "*", 2);
        {
            munit_assert_int(array->start_line, ==, 1);
            munit_assert_int(array->start_column, ==, 1);
            munit_assert_int(array->end_line, ==, 1);
            munit_assert_int(array->end_column, ==, 11);
        }
        program->free_fn((DaiAstBase*)program, true);
    }
    {
        const char* input = "[1, 2 * 2, 3 + 3];";
        DaiAstProgram prog;
        DaiAstProgram_init(&prog);
        DaiAstProgram* program = &prog;
        parse_helper(input, program);

        munit_assert_int(program->length, ==, 1);
        DaiAstStatement* stmt = program->statements[0];
        munit_assert_int(stmt->type, ==, DaiAstType_ExpressionStatement);
        DaiAstExpression* expr = ((DaiAstExpressionStatement*)stmt)->expression;
        munit_assert_int(expr->type, ==, DaiAstType_ArrayLiteral);
        const DaiAstArrayLiteral* array = (DaiAstArrayLiteral*)expr;
        munit_assert_size(array->length, ==, 3);
        check_integer_literal(array->elements[0], 1);
        check_infix_expression_int64(array->elements[1], 2, "*", 2);
        check_infix_expression_int64(array->elements[2], 3, "+", 3);
        {
            munit_assert_int(array->start_line, ==, 1);
            munit_assert_int(array->start_column, ==, 1);
            munit_assert_int(array->end_line, ==, 1);
            munit_assert_int(array->end_column, ==, 18);
        }
        program->free_fn((DaiAstBase*)program, true);
    }

    return MUNIT_OK;
}

static MunitResult
test_block_statement(__attribute__((unused)) const MunitParameter params[],
                     __attribute__((unused)) void* user_data) {
    const char* input = "{ var a = 5; var b = 10; var c = 15; };";
    DaiAstProgram prog;
    DaiAstProgram_init(&prog);
    DaiAstProgram* program = &prog;
    parse_helper(input, program);

    munit_assert_int(program->length, ==, 1);
    {
        DaiAstStatement* stmt = program->statements[0];
        munit_assert_int(stmt->type, ==, DaiAstType_BlockStatement);
        munit_assert_int(stmt->start_line, ==, 1);
        munit_assert_int(stmt->start_column, ==, 1);
        munit_assert_int(stmt->end_line, ==, 1);
        munit_assert_int(stmt->end_column, ==, 40);
    }
    DaiAstBlockStatement* block = (DaiAstBlockStatement*)program->statements[0];
    munit_assert_int(block->length, ==, 3);
    {
        DaiAstStatement* stmt = block->statements[0];
        munit_assert_int(stmt->type, ==, DaiAstType_VarStatement);
        {
            munit_assert_int(stmt->start_line, ==, 1);
            munit_assert_int(stmt->start_column, ==, 3);
            munit_assert_int(stmt->end_line, ==, 1);
            munit_assert_int(stmt->end_column, ==, 13);
        }
        DaiAstVarStatement* var_stmt = (DaiAstVarStatement*)stmt;
        check_identifier((DaiAstExpression*)var_stmt->name, "a");
        check_integer_literal(var_stmt->value, 5);
    }
    {
        DaiAstStatement* stmt = block->statements[1];
        munit_assert_int(stmt->type, ==, DaiAstType_VarStatement);
        {
            munit_assert_int(stmt->start_line, ==, 1);
            munit_assert_int(stmt->start_column, ==, 14);
            munit_assert_int(stmt->end_line, ==, 1);
            munit_assert_int(stmt->end_column, ==, 25);
        }
        DaiAstVarStatement* var_stmt = (DaiAstVarStatement*)stmt;
        check_identifier((DaiAstExpression*)var_stmt->name, "b");
        check_integer_literal(var_stmt->value, 10);
    }
    {
        DaiAstStatement* stmt = block->statements[2];
        munit_assert_int(stmt->type, ==, DaiAstType_VarStatement);
        {
            munit_assert_int(stmt->start_line, ==, 1);
            munit_assert_int(stmt->start_column, ==, 26);
            munit_assert_int(stmt->end_line, ==, 1);
            munit_assert_int(stmt->end_column, ==, 37);
        }
        DaiAstVarStatement* var_stmt = (DaiAstVarStatement*)stmt;
        check_identifier((DaiAstExpression*)var_stmt->name, "c");
        check_integer_literal(var_stmt->value, 15);
    }
    program->free_fn((DaiAstBase*)program, true);
    return MUNIT_OK;
}

static MunitResult
test_map_literal_expression(__attribute__((unused)) const MunitParameter params[],
                            __attribute__((unused)) void* user_data) {
    {

        const char* input = "var m = {};";
        DaiAstProgram prog;
        DaiAstProgram_init(&prog);
        DaiAstProgram* program = &prog;
        parse_helper(input, program);

        munit_assert_int(program->length, ==, 1);
        DaiAstStatement* stmt = program->statements[0];
        munit_assert_int(stmt->type, ==, DaiAstType_VarStatement);
        DaiAstVarStatement* var_stmt = (DaiAstVarStatement*)stmt;
        check_identifier((DaiAstExpression*)var_stmt->name, "m");
        DaiAstExpression* expr = var_stmt->value;
        munit_assert_int(expr->type, ==, DaiAstType_MapLiteral);
        const DaiAstMapLiteral* map = (DaiAstMapLiteral*)expr;
        munit_assert_size(map->length, ==, 0);
        {
            munit_assert_int(map->start_line, ==, 1);
            munit_assert_int(map->start_column, ==, 9);
            munit_assert_int(map->end_line, ==, 1);
            munit_assert_int(map->end_column, ==, 11);
        }
        program->free_fn((DaiAstBase*)program, true);
    }

    {
        const char* input = "var m = {\"foo\": 1};";
        DaiAstProgram prog;
        DaiAstProgram_init(&prog);
        DaiAstProgram* program = &prog;
        parse_helper(input, program);

        munit_assert_int(program->length, ==, 1);
        DaiAstStatement* stmt = program->statements[0];
        munit_assert_int(stmt->type, ==, DaiAstType_VarStatement);
        DaiAstVarStatement* var_stmt = (DaiAstVarStatement*)stmt;
        check_identifier((DaiAstExpression*)var_stmt->name, "m");
        DaiAstExpression* expr = var_stmt->value;
        munit_assert_int(expr->type, ==, DaiAstType_MapLiteral);
        const DaiAstMapLiteral* map = (DaiAstMapLiteral*)expr;
        munit_assert_size(map->length, ==, 1);
        check_string_literal(map->pairs[0].key, "\"foo\"");
        check_integer_literal(map->pairs[0].value, 1);
        {
            munit_assert_int(map->start_line, ==, 1);
            munit_assert_int(map->start_column, ==, 9);
            munit_assert_int(map->end_line, ==, 1);
            munit_assert_int(map->end_column, ==, 19);
        }
        program->free_fn((DaiAstBase*)program, true);
    }

    {
        const char* input = "var m = {\"foo\": 1, \"bar\": 2};";
        DaiAstProgram prog;
        DaiAstProgram_init(&prog);
        DaiAstProgram* program = &prog;
        parse_helper(input, program);

        munit_assert_int(program->length, ==, 1);
        DaiAstStatement* stmt = program->statements[0];
        munit_assert_int(stmt->type, ==, DaiAstType_VarStatement);
        DaiAstVarStatement* var_stmt = (DaiAstVarStatement*)stmt;
        check_identifier((DaiAstExpression*)var_stmt->name, "m");
        DaiAstExpression* expr = var_stmt->value;
        munit_assert_int(expr->type, ==, DaiAstType_MapLiteral);
        const DaiAstMapLiteral* map = (DaiAstMapLiteral*)expr;
        munit_assert_size(map->length, ==, 2);
        check_string_literal(map->pairs[0].key, "\"foo\"");
        check_integer_literal(map->pairs[0].value, 1);
        check_string_literal(map->pairs[1].key, "\"bar\"");
        check_integer_literal(map->pairs[1].value, 2);
        {
            munit_assert_int(map->start_line, ==, 1);
            munit_assert_int(map->start_column, ==, 9);
            munit_assert_int(map->end_line, ==, 1);
            munit_assert_int(map->end_column, ==, 29);
        }
        program->free_fn((DaiAstBase*)program, true);
    }

    {
        const char* input = "var m = {\"foo\": 1, \"bar\": 2, \"baz\": 3};";
        DaiAstProgram prog;
        DaiAstProgram_init(&prog);
        DaiAstProgram* program = &prog;
        parse_helper(input, program);

        munit_assert_int(program->length, ==, 1);
        DaiAstStatement* stmt = program->statements[0];
        munit_assert_int(stmt->type, ==, DaiAstType_VarStatement);
        DaiAstVarStatement* var_stmt = (DaiAstVarStatement*)stmt;
        check_identifier((DaiAstExpression*)var_stmt->name, "m");
        DaiAstExpression* expr = var_stmt->value;
        munit_assert_int(expr->type, ==, DaiAstType_MapLiteral);
        const DaiAstMapLiteral* map = (DaiAstMapLiteral*)expr;
        munit_assert_size(map->length, ==, 3);
        check_string_literal(map->pairs[0].key, "\"foo\"");
        check_integer_literal(map->pairs[0].value, 1);
        check_string_literal(map->pairs[1].key, "\"bar\"");
        check_integer_literal(map->pairs[1].value, 2);
        check_string_literal(map->pairs[2].key, "\"baz\"");
        check_integer_literal(map->pairs[2].value, 3);
        {
            munit_assert_int(map->start_line, ==, 1);
            munit_assert_int(map->start_column, ==, 9);
            munit_assert_int(map->end_line, ==, 1);
            munit_assert_int(map->end_column, ==, 39);
        }
        program->free_fn((DaiAstBase*)program, true);
    }

    return MUNIT_OK;
}


static MunitResult
test_forin_statement(__attribute__((unused)) const MunitParameter params[],
                     __attribute__((unused)) void* user_data) {
    {
        const char* input = "for (var i1, i2 in arr) { }";
        DaiAstProgram prog;
        DaiAstProgram_init(&prog);
        DaiAstProgram* program = &prog;
        parse_helper(input, program);

        munit_assert_int(program->length, ==, 1);
        DaiAstStatement* stmt = program->statements[0];
        munit_assert_int(stmt->type, ==, DaiAstType_ForInStatement);
        DaiAstForInStatement* forin_stmt = (DaiAstForInStatement*)stmt;
        {
            munit_assert_int(forin_stmt->start_line, ==, 1);
            munit_assert_int(forin_stmt->start_column, ==, 1);
            munit_assert_int(forin_stmt->end_line, ==, 1);
            munit_assert_int(forin_stmt->end_column, ==, 28);
        }
        check_identifier((DaiAstExpression*)forin_stmt->i, "i1");
        check_identifier((DaiAstExpression*)forin_stmt->e, "i2");
        check_identifier(forin_stmt->expression, "arr");
        munit_assert_int(forin_stmt->body->length, ==, 0);
        program->free_fn((DaiAstBase*)program, true);
    }
    {
        const char* input = "for (var e in arr) { }";
        DaiAstProgram prog;
        DaiAstProgram_init(&prog);
        DaiAstProgram* program = &prog;
        parse_helper(input, program);

        munit_assert_int(program->length, ==, 1);
        DaiAstStatement* stmt = program->statements[0];
        munit_assert_int(stmt->type, ==, DaiAstType_ForInStatement);
        DaiAstForInStatement* forin_stmt = (DaiAstForInStatement*)stmt;
        {
            munit_assert_int(forin_stmt->start_line, ==, 1);
            munit_assert_int(forin_stmt->start_column, ==, 1);
            munit_assert_int(forin_stmt->end_line, ==, 1);
            munit_assert_int(forin_stmt->end_column, ==, 23);
        }
        munit_assert_null(forin_stmt->i);
        check_identifier((DaiAstExpression*)forin_stmt->e, "e");
        check_identifier(forin_stmt->expression, "arr");
        munit_assert_int(forin_stmt->body->length, ==, 0);
        program->free_fn((DaiAstBase*)program, true);
    }

    return MUNIT_OK;
}

MunitTest parse_tests[] = {
    {(char*)"/test_var_statements", test_var_statements, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/test_return_statements",
     test_return_statements,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/test_if_statements", test_if_statements, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/test_assign_statements",
     test_assign_statements,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/test_if_else_statements",
     test_if_else_statements,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/test_if_elif_else_statements",
     test_if_elif_else_statements,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/test_while_statements",
     test_while_statements,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/test_class_statements",
     test_class_statements,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/test_identifier_expression",
     test_identifier_expression,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/test_integer_literal_expression",
     test_integer_literal_expression,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/test_float_literal_expression",
     test_float_literal_expression,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/test_syntax_error", test_syntax_error, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/test_function_literal_parsing",
     test_function_literal_parsing,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/test_function_parameter_parsing",
     test_function_parameter_parsing,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/test_string_literal_parsing",
     test_string_literal_parsing,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/test_parsing_prefix_expressions",
     test_parsing_prefix_expressions,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/test_parsing_infix_expressions",
     test_parsing_infix_expressions,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/test_parsing_call_expressions",
     test_parsing_call_expressions,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/test_parsing_call_expression_parameter_parsing",
     test_parsing_call_expression_parameter_parsing,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/test_operator_precedence_parsing",
     test_operator_precedence_parsing,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/test_boolean_expressions",
     test_boolean_expression,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/test_nil_expressions", test_nil_expression, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/test_function_statements",
     test_function_statements,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/test_parse_example", test_parse_example, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
    {(char*)"/test_array_literal_expression",
     test_array_literal_expression,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/test_block_statement",
     test_block_statement,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/test_map_literal_expression",
     test_map_literal_expression,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {(char*)"/test_forin_statement",
     test_forin_statement,
     NULL,
     NULL,
     MUNIT_TEST_OPTION_NONE,
     NULL},
    {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL},
};