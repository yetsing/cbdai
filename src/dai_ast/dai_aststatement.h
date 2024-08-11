#ifndef AF1F0FB7_2160_4F19_9BDE_8D075DABCEF8
#define AF1F0FB7_2160_4F19_9BDE_8D075DABCEF8

#include "dai_ast/dai_astbase.h"

#define DAI_AST_STATEMENT_HEAD \
    DaiAstType type;     \
    DaiAstStringFn string_fn; \
    DaiAstFreeFn free_fn; \
    int start_line; \
    int start_column; \
    int end_line; \
    int end_column;

typedef struct _DaiAstStatement {
    DAI_AST_STATEMENT_HEAD
} DaiAstStatement;

#endif /* AF1F0FB7_2160_4F19_9BDE_8D075DABCEF8 */
