#ifndef B8C8815A_AE82_4D97_878C_C7E54C6A1639
#define B8C8815A_AE82_4D97_878C_C7E54C6A1639

#include <stddef.h>

#include "dai_ast/dai_aststatement.h"

typedef struct {
    DAI_AST_STATEMENT_HEAD
    size_t size;
    size_t length;
    DaiAstStatement **statements;
} DaiAstBlockStatement;

DaiAstBlockStatement *
DaiAstBlockStatement_New(void);
void
DaiAstBlockStatement_append(DaiAstBlockStatement *block, DaiAstStatement *stmt);

#endif /* B8C8815A_AE82_4D97_878C_C7E54C6A1639 */
