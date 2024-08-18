#ifndef D8DC9D21_3030_4AA8_B393_10949166F172
#define D8DC9D21_3030_4AA8_B393_10949166F172

#include <stddef.h>

#include "dai_ast/dai_astblockstatement.h"
#include "dai_ast/dai_astidentifier.h"

typedef struct {
    DAI_AST_STATEMENT_HEAD
    char* name;
    size_t parameters_count;
    DaiAstIdentifier** parameters;
    DaiAstBlockStatement* body;
} DaiAstMethodStatement;

DaiAstMethodStatement*
DaiAstMethodStatement_New(void);

#endif /* D8DC9D21_3030_4AA8_B393_10949166F172 */
