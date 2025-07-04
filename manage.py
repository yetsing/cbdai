#!/usr/bin/env python3
import dataclasses
import os
import pathlib
import platform
import shutil
import subprocess
import sys
import time


@dataclasses.dataclass
class Global:
    cmake_command: str = ""
    ninja_command: str = ""
    llvm_binpath: str = "/usr/bin"
    os_name: str = platform.system().lower()


g = Global()


def find_cmake():
    g.cmake_command = os.getenv("CUSTOM_CMAKE_COMMAND", "")
    if not g.cmake_command:
        cmake_command = shutil.which("cmake")
        if not cmake_command:
            print("Not found cmake")
            sys.exit(1)
        g.cmake_command = cmake_command
    print(f"cmake_command: {g.cmake_command}")


def find_ninja():
    g.ninja_command = os.getenv("CUSTOM_NINJA_COMMAND", "")
    if not g.ninja_command:
        ninja_command = shutil.which("ninja")
        if not ninja_command:
            print("Not found ninja")
            sys.exit(1)
        g.ninja_command = ninja_command
    print(f"ninja_command: {g.ninja_command}")


def cp_compile_commands_json():
    # vscode clangd plugin needs this file for code hints
    subprocess.check_call(["cp", "./cmake-build-debug/compile_commands.json", "."])


def clean():
    subprocess.check_call(
        [g.cmake_command, "--build", "cmake-build-debug", "--target", "clean"]
    )


def compile(target, *args):
    # Compile
    cmake_args = [
        g.cmake_command,
        *args,
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=1",
        "-DCMAKE_BUILD_TYPE=Debug",
        f"-DCMAKE_MAKE_PROGRAM={g.ninja_command}",
        "-G",
        "Ninja",
        "-S",
        ".",
        "-B",
        "cmake-build-debug",
    ]
    subprocess.check_call(cmake_args)
    subprocess.check_call(
        [g.cmake_command, "--build", "cmake-build-debug", "--target", target, "-j", "9"]
    )


def test(*args):
    cmake_params = ["-DDAI_DEBUG_GC=OFF"]
    program_params = []

    for param in args:
        if param.startswith("-D"):
            cmake_params.append(param)
        else:
            program_params.append(param)

    if "-DDAI_TEST_VERBOSE=ON" not in cmake_params:
        # cmake 默认会缓存 option ，如果上次编译指定了 ON ，这次编译也会是 ON
        # 显式指定 OFF
        cmake_params.append("-DDAI_TEST_VERBOSE=OFF")

    compile("test", *cmake_params)
    cp_compile_commands_json()
    subprocess.check_call(["./cmake-build-debug/test", *program_params])
    subprocess.check_call(["time", "./cmake-build-debug/test", *program_params])

    print(
        "############## DEBUG GC #######################################################"
    )
    compile("test", "-DDAI_DEBUG_GC=ON")
    subprocess.check_call(["./cmake-build-debug/test", *program_params])

    print(
        "############## Sanitizer Test #################################################"
    )
    compile("santest")
    subprocess.check_call(["./cmake-build-debug/santest", *program_params])


def repl():
    compile("dai")
    subprocess.check_call(["./cmake-build-debug/Debug/dai"])


def show_ast(*args):
    compile("dai")
    subprocess.check_call(["./cmake-build-debug/Debug/dai", "ast", *args])


def benchmark(*args):
    compile("dai")
    subprocess.check_call(
        ["/usr/bin/time", "-v", "./cmake-build-debug/Debug/dai", *args]
    )


def benchmark_profile(*args):
    # todo
    compile("dai")
    subprocess.check_call(["./cmake-build-debug/Debug/dai", *args])
    subprocess.check_call(
        ["gprof", "./cmake-build-debug/Debug/dai", "gmon.out", ">benchmark.txt"]
    )


def mem():
    compile("test")
    subprocess.check_call(
        [
            "valgrind",
            "--tool=memcheck",
            "--leak-check=full",
            "--show-leak-kinds=all",
            "--trace-children=yes",
            "./cmake-build-debug/test",
            "--no-fork",
        ]
    )


def memrepl():
    compile("dai")
    subprocess.check_call(
        [
            "valgrind",
            "--tool=memcheck",
            "--leak-check=full",
            "--show-leak-kinds=all",
            "--trace-children=yes",
            "./cmake-build-debug/Debug/dai",
        ]
    )


def coverage(*args):
    compile("coverage", f"-DCMAKE_C_COMPILER={g.llvm_binpath}/clang")
    subprocess.check_call(
        [
            "LLVM_PROFILE_FILE=coverage.profraw",
            "./cmake-build-debug/coverage",
            "--no-fork",
        ]
    )
    subprocess.check_call(
        [
            f"{g.llvm_binpath}/llvm-profdata",
            "merge",
            "-sparse",
            "coverage.profraw",
            "-o",
            "coverage.profdata",
        ]
    )

    if args:
        output = subprocess.check_output(
            [
                f"{g.llvm_binpath}/llvm-cov",
                "show",
                "-show-expansions",
                "-show-branches=count",
                "-instr-profile=coverage.profdata",
                "./cmake-build-debug/coverage",
                *args,
                ">coverage.txt",
            ],
            text=True,
        )
    else:
        output = subprocess.check_output(
            [
                f"{g.llvm_binpath}/llvm-cov",
                "report",
                "./cmake-build-debug/coverage",
                "-instr-profile=coverage.profdata",
                ">coverage.txt",
            ],
            text=True,
        )
    pathlib.Path("coverage.txt").write_text(output)


def cfmt():
    clang_format_command = shutil.which("clang-format")
    if not clang_format_command:
        print("clang-format could not be found")
        sys.exit(1)

    patterns = [
        "test/*.c",
        "test/*.h",
        "src/atstr/*",
        "src/dai_*.c",
        "src/dai_*.h",
        "src/dai_ast/*.c",
        "src/dai_ast/*.h",
        "dai/*.c",
        "dai/*.h",
        "dai/std/*.c",
        "dai/std/*.h",
    ]
    paths = []
    # glob with patterns
    for pattern in patterns:
        paths.extend(pathlib.Path(".").glob(pattern))
    for path in paths:
        subprocess.check_call([clang_format_command, "-i", "--verbose", path])


def runfile(*args):
    compile("dai")
    cp_compile_commands_json()
    exitcode = 0
    run_forever = os.environ.get("DAI_RUN_FOREVER", "0") != "0"
    if run_forever:
        print("Running forever, press Ctrl+C to stop")
    while True:
        child = subprocess.Popen(["./cmake-build-debug/Debug/dai", *args])
        while True:
            try:
                exitcode = child.wait()
                break
            except KeyboardInterrupt:
                print("Ctrl+C")
                run_forever = False
        if not run_forever:
            break
        print("Restarting...")
        if exitcode != 0:
            print(f"Exit code: {exitcode}")
            input("Press Enter to continue...")
    if exitcode != 0:
        print(f"Exit code: {exitcode}")
        sys.exit(exitcode)


def run_and_monitor(cmd, interval=1) -> int:
    import matplotlib.pyplot as plt
    import psutil

    # Start the process.
    print(f"Running command: {cmd}")
    proc = subprocess.Popen(cmd)
    p = psutil.Process(proc.pid)

    times = []
    mem_usage = []
    start_time = time.time()

    try:
        next_time = start_time + interval
        while proc.poll() is None:
            if time.time() > next_time:
                next_time = time.time() + interval
                try:
                    # Get RSS memory in MB.
                    mem = p.memory_info().rss / (1024 * 1024)
                except psutil.NoSuchProcess:
                    break
                current_time = time.time() - start_time
                times.append(current_time // interval)
                mem_usage.append(mem)
                print("Time: {:.2f}s  Memory: {:.2f} MB".format(current_time, mem))
            time.sleep(1)
    except KeyboardInterrupt:
        print("Ctrl+C")
        proc.terminate()
        proc.wait()

    # Final reading after process ends.
    if proc.poll() is not None:
        current_time = time.time() - start_time
        try:
            mem = p.memory_info().rss / (1024 * 1024)
        except psutil.NoSuchProcess:
            mem = 0
        times.append(current_time)
        mem_usage.append(mem)
        print("Time: {:.2f}s  Memory: {:.2f} MB".format(current_time, mem))

    # Draw memory usage chart.
    plt.figure(figsize=(10, 5))
    plt.plot(times, mem_usage, marker="o")
    plt.xlabel("Time (s)")
    plt.ylabel("Memory Usage (MB)")
    plt.title("Memory Usage Over Time")
    plt.grid(True)
    plt.savefig("memory_usage.png")

    return proc.returncode


def runfilem(*args):
    compile("dai")
    cp_compile_commands_json()
    exitcode = run_and_monitor(
        ["./cmake-build-debug/Debug/dai", *args],
        interval=60,
    )
    if exitcode != 0:
        print(f"Exit code: {exitcode}")
        sys.exit(exitcode)


def dis(*args):
    compile("dai")
    subprocess.check_call(["./cmake-build-debug/Debug/dai", "dis", *args])


def fmt(*args):
    compile("dai")
    subprocess.check_call(["./cmake-build-debug/Debug/dai", "fmt", *args])


def fmt_check(*args):
    compile("dai")
    subprocess.check_call(["./cmake-build-debug/Debug/dai", "fmt_check", *args])


def main():
    find_cmake()
    find_ninja()

    if len(sys.argv) < 2:
        print("No command provided")
        sys.exit(1)

    command = sys.argv[1]
    args = sys.argv[2:]

    commands = {
        "cfmt": cfmt,
        "clean": clean,
        "test": test,
        "repl": repl,
        "show_ast": show_ast,
        "benchmark": benchmark,
        "benchmark_profile": benchmark_profile,
        "mem": mem,
        "memrepl": memrepl,
        "coverage": coverage,
        "runfile": runfile,
        "runfilem": runfilem,
        "dis": dis,
        "fmt": fmt,
        "fmt_check": fmt_check,
    }

    if command in commands:
        commands[command](*args)
    else:
        print(f"Unknown command: {command}")
        sys.exit(1)


if __name__ == "__main__":
    main()
