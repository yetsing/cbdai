"""
裁剪 coverage output
"""

import sys
import typing

# 排除已经解决的文件
exclude_keywords = [
    "test/test",
    "munit",
    "main.c",
    "munit/munit.c",
    "munit/munit.h",
    "src/dai_assert.h",
    "src/dai_malloc.h",
    "Files which contain no functions:",
    "src/dai_parseint.c",
    "src/dai_malloc.c",
    "src/dai_parse.c",
    "src/dai_ast/dai_asttype.c",
    "src/dai_parser/dai_parseprogram.h",
    "src/dai_parser/dai_parsercommon.h",
    "src/dai_parser/dai_parserregister.h",
    "src/dai_parser/dai_parsestatement.h",
    "src/dai_tokenize.c",
    "src/dai_parser/dai_parseExpressionStatement.h",
    "src/dai_parser/dai_parseBoolean.h",
    "src/dai_parser/dai_parseexpression.h",
    "src/dai_parser/dai_parsePrefixExpression.h",
    "src/dai_parser/dai_parseVarStatement.h",
    "src/dai_ast/dai_astvarstatement.c",
    "src/dai_stringbuffer.c",
    "src/dai_parser/dai_parseCallExpression.h",
    "src/dai_parser/dai_parseblockstatement.h",
    "src/dai_parser/dai_parserbase.h",
    "src/dai_parser/dai_parseIfStatement.h",
    "src/dai_ast/dai_astblockstatement.c",
    "src/dai_ast/dai_astinfixexpression.c",
    "src/dai_ast/dai_astfunctionliteral.c",
    "src/dai_parser/dai_parseIdentifier.h",
]


def is_excluded(line):
    return False
    for keyword in exclude_keywords:
        if keyword in line:
            return True
    return False


def sort_key_fn(item: typing.Tuple[int, str]) -> typing.Tuple:
    index, line = item
    parts = s.split()
    if len(parts) == 13:
        return parts[-1]
    else:
        return "999"


def main():
    filename = sys.argv[1]
    with open(filename, "r", encoding="utf-8") as f:
        lines = []
        for line in f:
            if is_excluded(line):
                continue
            lines.append(line)

    order_lines = [(i, line) for i, line in enumerate(lines)]
    start = 2
    end = len(lines) - 2
    item_lines = lines[start:end]
    item_lines.sort(key=sort_key_fn, reverse=True)
    with open(filename, "w", encoding="utf-8") as f:
        f.writelines(lines[:start])
        f.writelines(item_lines)
        f.writelines(lines[end:])


main()
