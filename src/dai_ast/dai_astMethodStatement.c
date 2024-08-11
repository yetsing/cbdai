#include <assert.h>

#include "dai_ast/dai_astcommon.h"
#include "dai_ast/dai_astMethodStatement.h"
#include "dai_malloc.h"
#include "dai_stringbuffer.h"

static char *DaiAstMethodStatement_string(DaiAstBase *base, bool recursive) {
  assert(base->type == DaiAstType_MethodStatement);
  DaiAstMethodStatement *stmt = (DaiAstMethodStatement *)base;
  DaiStringBuffer *sb = DaiStringBuffer_New();
  DaiStringBuffer_write(sb, "{\n");
  DaiStringBuffer_write(sb, indent);
  // DaiStringBuffer_write(sb, "type: DaiAstType_MethodStatement,\n");
  DaiStringBuffer_write(sb, KEY_COLOR("type") ": " TYPE_COLOR("DaiAstType_MethodStatement") ",\n");
  DaiStringBuffer_writef(sb, "%sname: %s,\n", indent, stmt->name);
  {
    DaiStringBuffer_write(sb, indent);
    DaiStringBuffer_write(sb, "parameters: [");
    for (size_t i = 0; i < stmt->parameters_count; i++) {
      DaiAstIdentifier *param = stmt->parameters[i];
      DaiStringBuffer_write(sb, param->value);
      DaiStringBuffer_write(sb, ", ");
    }
    DaiStringBuffer_write(sb, "],\n");
  }

  {
    DaiStringBuffer_write(sb, indent);
    DaiStringBuffer_write(sb, "body: ");
    if (recursive) {
      char *s = stmt->body->string_fn((DaiAstBase *)stmt->body, recursive);
      DaiStringBuffer_writeWithLinePrefix(sb, s, indent);
      dai_free(s);
    } else {
      DaiStringBuffer_writePointer(sb, stmt->body);
    }
    DaiStringBuffer_write(sb, ",\n");
  }
  DaiStringBuffer_write(sb, "}");
  char *s = DaiStringBuffer_getAndFree(sb, NULL);
  return s;
}

static void DaiAstMethodStatement_free(DaiAstBase *base, bool recursive) {
  assert(base->type == DaiAstType_MethodStatement);
  DaiAstMethodStatement *stmt = (DaiAstMethodStatement *)base;
  if (stmt->parameters != NULL) {
    if (recursive) {
      for (size_t i = 0; i < stmt->parameters_count; i++) {
        stmt->parameters[i]->free_fn((DaiAstBase *)stmt->parameters[i], true);
      }
    }
    dai_free(stmt->parameters);
  }
  if (stmt->body != NULL) {
    if (recursive) {
      stmt->body->free_fn((DaiAstBase *)stmt->body, true);
    }
  }
  dai_free(stmt->name);
  dai_free(base);
}

DaiAstMethodStatement *DaiAstMethodStatement_New(void) {
  DaiAstMethodStatement *f = dai_malloc(sizeof(DaiAstMethodStatement));
  f->name = NULL;
  f->parameters_count = 0;
  f->parameters = NULL;
  f->body = NULL;
  {
    f->type = DaiAstType_MethodStatement;
    f->free_fn = DaiAstMethodStatement_free;
    f->string_fn = DaiAstMethodStatement_string;
  }
  {
    f->start_line = 0;
    f->start_column = 0;
    f->end_line = 0;
    f->end_column = 0;
  }
  return f;
}
