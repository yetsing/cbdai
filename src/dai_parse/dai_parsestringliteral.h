#ifndef DA14413D_5880_41B8_BA3D_A4E841277D0F
#define DA14413D_5880_41B8_BA3D_A4E841277D0F

static DaiAstExpression *Parser_parseStringLiteral(Parser *p) {
    return (DaiAstExpression *) DaiAstStringLiteral_New(p->cur_token);
}

#endif /* DA14413D_5880_41B8_BA3D_A4E841277D0F */
