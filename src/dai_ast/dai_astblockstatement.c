#include <assert.h>

#include "dai_ast/dai_astblockstatement.h"
#include "dai_ast/dai_astcommon.h"
#include "dai_malloc.h"
#include "dai_stringbuffer.h"

static char *DaiAstBlockStatement_string(DaiAstBase *base, bool recursive) {
  assert(base->type == DaiAstType_BlockStatement);
  DaiAstBlockStatement *block = (DaiAstBlockStatement *)base;
  DaiStringBuffer *sb = DaiStringBuffer_New();
  DaiStringBuffer_write(sb, "{\n");
  DaiStringBuffer_write(sb, indent);
  // DaiStringBuffer_write(sb, "type: DaiAstType_BlockStatement,\n");
    DaiStringBuffer_write(sb, KEY_COLOR("type") ": " TYPE_COLOR("DaiAstType_BlockStatement") ",\n");
  DaiStringBuffer_write(sb, indent);
  DaiStringBuffer_write(sb, "length: ");
  DaiStringBuffer_writeInt64(sb, block->length);
  DaiStringBuffer_write(sb, ",\n");
  if (recursive) {
    DaiStringBuffer_write(sb, indent);
    DaiStringBuffer_write(sb, "statements: [\n");
    DaiAstStatement **statements = block->statements;
    for (size_t i = 0; i < block->length; i++) {
      char *s = statements[i]->string_fn((DaiAstBase *)statements[i], true);
      DaiStringBuffer_writeWithLinePrefix(sb, s, doubleindent);
      DaiStringBuffer_write(sb, ",\n");
      dai_free(s);
    }
    DaiStringBuffer_write(sb, indent);
    DaiStringBuffer_write(sb, "],\n");
  } else {
    DaiStringBuffer_write(sb, indent);
    DaiStringBuffer_write(sb, "statements: [...],\n");
  }
  DaiStringBuffer_write(sb, "}");
  return DaiStringBuffer_getAndFree(sb, NULL);
}

void DaiAstBlockStatement_free(DaiAstBase *base, bool recursive) {
  assert(base->type == DaiAstType_BlockStatement);
  DaiAstBlockStatement *block = (DaiAstBlockStatement *)base;
  if (recursive) {
    for (size_t i = 0; i < block->length; i++) {
      block->statements[i]->free_fn((DaiAstBase *)block->statements[i], recursive);
    }
  }
  dai_free(block->statements);
  dai_free(block);
}

DaiAstBlockStatement *DaiAstBlockStatement_New(void) {
  DaiAstBlockStatement *block = dai_malloc(sizeof(DaiAstBlockStatement));
  block->type = DaiAstType_BlockStatement;
  {
    block->string_fn = DaiAstBlockStatement_string;
    block->free_fn = DaiAstBlockStatement_free;
  }
  block->size = 0;
  block->length = 0;
  block->statements = NULL;
  return block;
}

void DaiAstBlockStatement_append(DaiAstBlockStatement *block,
                                 DaiAstStatement *stmt) {
  if (block->length + 1 > block->size) {
    size_t newsize = block->size < 8 ? 8 : (block->size * 2);
    block->statements = (DaiAstStatement **)dai_realloc(
        block->statements, sizeof(DaiAstStatement *) * newsize);
    block->size = newsize;
  }
  block->statements[block->length] = stmt;
  block->length++;
}