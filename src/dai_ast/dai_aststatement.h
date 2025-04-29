#ifndef CBDAI_DAI_ASTSTATEMENT_H
#define CBDAI_DAI_ASTSTATEMENT_H

#include "dai_ast/dai_astbase.h"

#define DAI_AST_STATEMENT_HEAD \
    DAI_AST_BASE_HEAD          \
    int start_line;            \
    int start_column;          \
    int end_line;              \
    int end_column;

typedef struct _DaiAstStatement {
    DAI_AST_STATEMENT_HEAD
} DaiAstStatement;

#endif /* CBDAI_DAI_ASTSTATEMENT_H */
