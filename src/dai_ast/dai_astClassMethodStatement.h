#ifndef E589CD75_711C_4911_AC51_EB9CD2E268F3
#define E589CD75_711C_4911_AC51_EB9CD2E268F3

#include <stddef.h>

#include "dai_array.h"
#include "dai_ast/dai_astblockstatement.h"
#include "dai_ast/dai_astidentifier.h"

typedef struct {
    DAI_AST_STATEMENT_HEAD
    char* name;
    size_t parameters_count;
    DaiAstIdentifier** parameters;
    DaiArray* defaults;   // DaiAstExpression* array
    DaiAstBlockStatement* body;
} DaiAstClassMethodStatement;

DaiAstClassMethodStatement*
DaiAstClassMethodStatement_New(void);

#endif /* E589CD75_711C_4911_AC51_EB9CD2E268F3 */
