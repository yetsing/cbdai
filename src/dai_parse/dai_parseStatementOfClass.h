#ifndef DBE29593_3646_4189_9CA9_2DCA7E014574
#define DBE29593_3646_4189_9CA9_2DCA7E014574

#include "dai_parse/dai_parseInsVarStatement.h"
#include "dai_parse/dai_parseMethodStatement.h"
#include "dai_parse/dai_parseClassVarStatement.h"
#include "dai_parse/dai_parseClassMethodStatement.h"

static DaiAstStatement *Parser_parseStatementOfClass(Parser *p) {
    switch (p->cur_token->type) {
        case DaiTokenType_class: {
            // class var 语句
            if (Parser_peekTokenIs(p, DaiTokenType_var)) {
               return (DaiAstStatement *)Parser_parseClassVarStatement(p);
            }
            if (Parser_peekTokenIs(p, DaiTokenType_function)) {
                return (DaiAstStatement *)Parser_parseClassMethodStatement(p);
            }
            break;
        }
        case DaiTokenType_var: {
            // var 语句
            return (DaiAstStatement *)Parser_parseInsVarStatement(p);
        }
        case DaiTokenType_function: {
            // method 语句
            return (DaiAstStatement *) Parser_parseMethodStatement(p);
        }
        default: {
            break;
        }
    }
    p->syntax_error = DaiSyntaxError_New("invalid statement in class scope", p->cur_token->start_line, p->cur_token->start_column);
    return NULL;
}

#endif /* DBE29593_3646_4189_9CA9_2DCA7E014574 */
