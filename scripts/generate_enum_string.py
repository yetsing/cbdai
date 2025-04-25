"""
生成 C 语言中枚举的字符串表示
"""

import sys
import pathlib


def main():
    filename = sys.argv[1]
    with open(filename, "r", encoding="utf-8") as f:
        found_enum = False
        enum_lines = []
        for line in f:
            line = line.strip()
            if not line:
                continue
            if "enum" in line:
                found_enum = True
                continue
            if found_enum and line.startswith("}"):
                found_enum = False
                enum_name = line[line.index("}") + 1 : -1].strip()
                strings = []
                for v in enum_lines:
                    strings.append(f'    "{v}",')
                body = "\n".join(strings)
                print(f"static const char *{enum_name}Strings[] = {{\n{body}\n}};")
                continue

            if found_enum:
                if line.startswith("//"):
                    continue
                if "=" in line:
                    line = line[: line.index("=")].strip()
                if "//" in line:
                    line = line[: line.index("//")].strip()
                enum_lines.append(line.strip(", "))


main()
