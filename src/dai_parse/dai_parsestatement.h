//
// Created by  on 2024/6/5.
//

#ifndef CBDAI_SRC_DAI_PARSER_DAI_PARSESTATEMENT_H_
#define CBDAI_SRC_DAI_PARSER_DAI_PARSESTATEMENT_H_

#include "dai_parse/dai_parseExpressionOrAssignStatement.h"
#include "dai_parse/dai_parseFunctionStatement.h"
#include "dai_parse/dai_parseassignstatement.h"
#include "dai_parse/dai_parseblockstatement.h"
#include "dai_parse/dai_parseclassstatement.h"
#include "dai_parse/dai_parseexpressionstatement.h"
#include "dai_parse/dai_parseifstatement.h"
#include "dai_parse/dai_parsereturnstatement.h"
#include "dai_parse/dai_parsevarstatement.h"

static DaiAstStatement*
Parser_parseStatement(Parser* p) {
    switch (p->cur_token->type) {
        case DaiTokenType_class: {
            // class 语句
            return (DaiAstStatement*)Parser_parseClassStatement(p);
        }
        case DaiTokenType_var: {
            // var 语句
            return (DaiAstStatement*)Parser_parseVarStatement(p);
        }
        case DaiTokenType_return: {
            // return 语句
            return (DaiAstStatement*)Parser_parseReturnStatement(p);
        }
        case DaiTokenType_if: {
            // if 语句
            return (DaiAstStatement*)Parser_parseIfStatement(p);
        }
        case DaiTokenType_function: {
            if (Parser_peekTokenIs(p, DaiTokenType_lparen)) {
                // 表达式语句
                return (DaiAstStatement*)Parser_parseExpressionStatement(p);
            } else {
                // function 语句
                return (DaiAstStatement*)Parser_parseFunctionStatement(p);
            }
        }
        case DaiTokenType_self:
        case DaiTokenType_super:
        case DaiTokenType_ident: {
            return Parser_parseExpressionOrAssignStatement(p);
        }
        default: {
            return (DaiAstStatement*)Parser_parseExpressionStatement(p);
            break;
        }
    }
    return NULL;
}

#endif   // CBDAI_SRC_DAI_PARSER_DAI_PARSESTATEMENT_H_
