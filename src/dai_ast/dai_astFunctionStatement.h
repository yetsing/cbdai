#ifndef C3D21FC6_7BA3_4426_9C0E_1E8D17AABC74
#define C3D21FC6_7BA3_4426_9C0E_1E8D17AABC74

#include <stddef.h>

#include "dai_ast/dai_astblockstatement.h"
#include "dai_ast/dai_astidentifier.h"

typedef struct {
    DAI_AST_STATEMENT_HEAD
    char* name;
    size_t parameters_count;
    DaiAstIdentifier** parameters;
    DaiAstBlockStatement* body;
} DaiAstFunctionStatement;

DaiAstFunctionStatement*
DaiAstFunctionStatement_New(void);

#endif /* C3D21FC6_7BA3_4426_9C0E_1E8D17AABC74 */
