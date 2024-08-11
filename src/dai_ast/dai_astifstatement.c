#include <assert.h>

#include "dai_ast/dai_astifstatement.h"
#include "dai_ast/dai_astcommon.h"
#include "dai_malloc.h"
#include "dai_stringbuffer.h"

static char *DaiAstIfStatement_string(DaiAstBase *base, bool recursive) {
  assert(base->type == DaiAstType_IfStatement);
  DaiAstIfStatement *ifstatement = (DaiAstIfStatement *)base;
  DaiStringBuffer *sb = DaiStringBuffer_New();
  DaiStringBuffer_write(sb, "{\n");
  DaiStringBuffer_write(sb, indent);
  // DaiStringBuffer_write(sb, "type: DaiAstType_IfStatement,\n");
  DaiStringBuffer_write(sb, KEY_COLOR("type") ": " TYPE_COLOR("DaiAstType_IfStatement") ",\n");
  DaiStringBuffer_write(sb, indent);
  DaiStringBuffer_write(sb, "condition: ");
  if (recursive) {
    char *s = ifstatement->condition->string_fn(
        (DaiAstBase *)ifstatement->condition, recursive);
    DaiStringBuffer_writeWithLinePrefix(sb, s, indent);
    DaiStringBuffer_write(sb, ",\n");
    dai_free(s);
  }
  DaiStringBuffer_write(sb, indent);
  DaiStringBuffer_write(sb, "then_branch: ");
  if (recursive) {
    char *s = ifstatement->then_branch->string_fn(
        (DaiAstBase *)ifstatement->then_branch, recursive);
    DaiStringBuffer_writeWithLinePrefix(sb, s, indent);
    DaiStringBuffer_write(sb, ",\n");
    dai_free(s);
  } else {
    DaiStringBuffer_writePointer(sb, ifstatement->then_branch);
    DaiStringBuffer_write(sb, ",\n");
  }
  if (ifstatement->else_branch != NULL) {
    DaiStringBuffer_write(sb, indent);
    DaiStringBuffer_write(sb, "else_branch: ");
    if (recursive) {
      char *s = ifstatement->else_branch->string_fn(
          (DaiAstBase *)ifstatement->else_branch, recursive);
      DaiStringBuffer_writeWithLinePrefix(sb, s, indent);
      DaiStringBuffer_write(sb, ",\n");
      dai_free(s);
    } else {
      DaiStringBuffer_writePointer(sb, ifstatement->else_branch);
      DaiStringBuffer_write(sb, ",\n");
    }
  }
  DaiStringBuffer_write(sb, "}");
  char *s = DaiStringBuffer_getAndFree(sb, NULL);
  return s;
}

void DaiAstIfStatement_free(DaiAstBase *base, bool recursive) {
  assert(base->type == DaiAstType_IfStatement);
  DaiAstIfStatement *ifstatement = (DaiAstIfStatement *)base;
  if (recursive) {
    if (ifstatement->condition != NULL) {
      ifstatement->condition->free_fn((DaiAstBase *)ifstatement->condition, true);
    }
    if (ifstatement->then_branch != NULL) {
      ifstatement->then_branch->free_fn((DaiAstBase *)ifstatement->then_branch,
                                        true);
    }
    if (ifstatement->else_branch != NULL) {
      ifstatement->else_branch->free_fn((DaiAstBase *)ifstatement->else_branch,
                                        true);
    }
  }
  dai_free(ifstatement);
}

DaiAstIfStatement *DaiAstIfStatement_New(void) {
  DaiAstIfStatement *ifstatement = dai_malloc(sizeof(DaiAstIfStatement));
  ifstatement->type = DaiAstType_IfStatement;
  ifstatement->condition = NULL;
  ifstatement->then_branch = NULL;
  ifstatement->else_branch = NULL;
  {
    ifstatement->string_fn = DaiAstIfStatement_string;
    ifstatement->free_fn = DaiAstIfStatement_free;
  }
  {
    ifstatement->start_line = 0;
    ifstatement->start_column = 0;
    ifstatement->end_line = 0;
    ifstatement->end_column = 0;
  }
  return ifstatement;
}