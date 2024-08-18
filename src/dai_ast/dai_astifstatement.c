#include <assert.h>

#include "dai_ast/dai_astcommon.h"
#include "dai_ast/dai_astifstatement.h"
#include "dai_malloc.h"
#include "dai_stringbuffer.h"

static char*
DaiAstIfStatement_string(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_IfStatement);
    DaiAstIfStatement* ifstatement = (DaiAstIfStatement*)base;
    DaiStringBuffer* sb            = DaiStringBuffer_New();
    DaiStringBuffer_write(sb, "{\n");
    DaiStringBuffer_write(sb, indent);
    // DaiStringBuffer_write(sb, "type: DaiAstType_IfStatement,\n");
    DaiStringBuffer_write(sb, KEY_COLOR("type") ": " TYPE_COLOR("DaiAstType_IfStatement") ",\n");
    DaiStringBuffer_write(sb, indent);
    DaiStringBuffer_write(sb, "condition: ");
    if (recursive) {
        char* s = ifstatement->condition->string_fn((DaiAstBase*)ifstatement->condition, recursive);
        DaiStringBuffer_writeWithLinePrefix(sb, s, indent);
        DaiStringBuffer_write(sb, ",\n");
        dai_free(s);
    }
    DaiStringBuffer_write(sb, indent);
    DaiStringBuffer_write(sb, "then_branch: ");
    if (recursive) {
        char* s =
            ifstatement->then_branch->string_fn((DaiAstBase*)ifstatement->then_branch, recursive);
        DaiStringBuffer_writeWithLinePrefix(sb, s, indent);
        DaiStringBuffer_write(sb, ",\n");
        dai_free(s);
    } else {
        DaiStringBuffer_writePointer(sb, ifstatement->then_branch);
        DaiStringBuffer_write(sb, ",\n");
    }
    DaiStringBuffer_write(sb, indent);
    DaiStringBuffer_write(sb, "elif_branches: [\n");
    for (int i = 0; i < ifstatement->elif_branch_count; i++) {
        if (recursive) {
            DaiStringBuffer_write(sb, indent);
            DaiStringBuffer_writef(sb, "[%d]\n", i);
            {
                char* s = ifstatement->elif_branches[i].condition->string_fn(
                    (DaiAstBase*)ifstatement->elif_branches[i].condition, recursive);
                DaiStringBuffer_writeWithLinePrefix(sb, s, indent);
                DaiStringBuffer_write(sb, ",\n");
                dai_free(s);
            }
            {
                char* s = ifstatement->elif_branches[i].then_branch->string_fn(
                    (DaiAstBase*)ifstatement->elif_branches[i].then_branch, recursive);
                DaiStringBuffer_writeWithLinePrefix(sb, s, indent);
                DaiStringBuffer_write(sb, ",\n");
                dai_free(s);
            }
        } else {
            DaiStringBuffer_writeInt(sb, ifstatement->elif_branch_count);
        }
    }
    DaiStringBuffer_write(sb, indent);
    DaiStringBuffer_write(sb, "],\n");
    if (ifstatement->else_branch != NULL) {
        DaiStringBuffer_write(sb, indent);
        DaiStringBuffer_write(sb, "else_branch: ");
        if (recursive) {
            char* s = ifstatement->else_branch->string_fn((DaiAstBase*)ifstatement->else_branch,
                                                          recursive);
            DaiStringBuffer_writeWithLinePrefix(sb, s, indent);
            DaiStringBuffer_write(sb, ",\n");
            dai_free(s);
        } else {
            DaiStringBuffer_writePointer(sb, ifstatement->else_branch);
            DaiStringBuffer_write(sb, ",\n");
        }
    }
    DaiStringBuffer_write(sb, "}");
    char* s = DaiStringBuffer_getAndFree(sb, NULL);
    return s;
}

void
DaiAstIfStatement_free(DaiAstBase* base, bool recursive) {
    assert(base->type == DaiAstType_IfStatement);
    DaiAstIfStatement* ifstatement = (DaiAstIfStatement*)base;
    if (recursive) {
        if (ifstatement->condition != NULL) {
            ifstatement->condition->free_fn((DaiAstBase*)ifstatement->condition, true);
        }
        if (ifstatement->then_branch != NULL) {
            ifstatement->then_branch->free_fn((DaiAstBase*)ifstatement->then_branch, true);
        }
        if (ifstatement->elif_branches != NULL) {
            for (int i = 0; i < ifstatement->elif_branch_count; i++) {
                ifstatement->elif_branches[i].condition->free_fn(
                    (DaiAstBase*)ifstatement->elif_branches[i].condition, true);
                ifstatement->elif_branches[i].then_branch->free_fn(
                    (DaiAstBase*)ifstatement->elif_branches[i].then_branch, true);
            }
        }
        if (ifstatement->else_branch != NULL) {
            ifstatement->else_branch->free_fn((DaiAstBase*)ifstatement->else_branch, true);
        }
    }
    dai_free(ifstatement->elif_branches);
    dai_free(ifstatement);
}

DaiAstIfStatement*
DaiAstIfStatement_New(void) {
    DaiAstIfStatement* ifstatement    = dai_malloc(sizeof(DaiAstIfStatement));
    ifstatement->type                 = DaiAstType_IfStatement;
    ifstatement->condition            = NULL;
    ifstatement->then_branch          = NULL;
    ifstatement->else_branch          = NULL;
    ifstatement->elif_branch_capacity = 0;
    ifstatement->elif_branch_count    = 0;
    ifstatement->elif_branches        = NULL;
    {
        ifstatement->string_fn = DaiAstIfStatement_string;
        ifstatement->free_fn   = DaiAstIfStatement_free;
    }
    {
        ifstatement->start_line   = 0;
        ifstatement->start_column = 0;
        ifstatement->end_line     = 0;
        ifstatement->end_column   = 0;
    }
    return ifstatement;
}
void
DaiAstIfStatement_append_elif_branch(DaiAstIfStatement* ifstatement, DaiAstExpression* condition,
                                     DaiAstBlockStatement* then_branch) {
    if (ifstatement->elif_branch_count >= ifstatement->elif_branch_capacity) {
        ifstatement->elif_branch_capacity = GROW_CAPACITY(ifstatement->elif_branch_capacity);
        DaiBranch* new_branches           = dai_realloc(
            ifstatement->elif_branches, sizeof(DaiBranch) * ifstatement->elif_branch_capacity);
        ifstatement->elif_branches = new_branches;
    }
    ifstatement->elif_branches[ifstatement->elif_branch_count].condition   = condition;
    ifstatement->elif_branches[ifstatement->elif_branch_count].then_branch = then_branch;
    ifstatement->elif_branch_count++;
}
