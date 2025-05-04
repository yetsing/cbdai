import os
import sys


def unify(directory):
    for root, _, files in os.walk(directory):
        for file in files:
            if file.startswith("dai") and file.endswith(".h"):
                file_path = os.path.join(root, file)
                lines = []
                with open(file_path, "r") as f:
                    lines = f.readlines()
                if len(lines) < 3:
                    continue
                guard = f"CBDAI_{file.replace('.h', '_H').upper()}"
                for i in range(len(lines) - 1):
                    if lines[i].startswith("#ifndef") and lines[i + 1].startswith(
                        "#define"
                    ):
                        lines[i] = f"#ifndef {guard}\n"
                        lines[i + 1] = f"#define {guard}\n"
                        break
                for i in range(len(lines) - 1, 2, -1):
                    if lines[i].startswith("#endif"):
                        lines[i] = f"#endif /* {guard} */\n"
                        break
                with open(file_path, "w") as f:
                    f.writelines(lines)
                print(f"Updated include guard in {file_path} to {guard}")


def main():
    unify(sys.argv[1])


if __name__ == "__main__":
    main()
