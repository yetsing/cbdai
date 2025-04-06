#ifndef C8CDB464_FD15_489C_8AC8_AE125875BA7E
#define C8CDB464_FD15_489C_8AC8_AE125875BA7E

#include "dai_ast/dai_astidentifier.h"
#include "dai_ast/dai_aststatement.h"

typedef struct {
    DAI_AST_STATEMENT_HEAD
    bool is_con;
    DaiAstIdentifier* name;
    DaiAstExpression* value;
} DaiAstClassVarStatement;

DaiAstClassVarStatement*
DaiAstClassVarStatement_New(DaiAstIdentifier* name, DaiAstExpression* value);

#endif /* C8CDB464_FD15_489C_8AC8_AE125875BA7E */
