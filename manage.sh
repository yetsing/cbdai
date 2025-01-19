#!/usr/bin/env bash

set -e
# set -x

cmake_command=""
ninja_command=""
llvm_binpath="/usr/bin"

os_name="unknown"

function find_os_name() {
	case "$OSTYPE" in
	solaris*) os_name="solaris" ;;
	darwin*) os_name="darwin" ;;
	linux*) os_name="linux" ;;
	bsd*) os_name="bsd" ;;
	msys*) os_name="windows" ;;
	cygwin*) os_name="windows" ;;
	*) echo "unknown: $OSTYPE" ;;
	esac
}

function find_cmake() {
	cmake_command="${CUSTOM_CMAKE_COMMAND}"
	if [ -z "${cmake_command}" ]; then
		if command -v cmake 1>/dev/null 2>/dev/null; then
			cmake_command=$(command -v cmake)
		else
			echo "Not found cmake"
			return 1
		fi
	fi
	echo "cmake_command ${cmake_command}"
}

function find_ninja() {
	ninja_command="${CUSTOM_NINJA_COMMAND}"
	if [ -z "${ninja_command}" ]; then
		if command -v ninja 1>/dev/null 2>/dev/null; then
			ninja_command=$(command -v ninja)
		else
			echo "Not found ninja"
			return 1
		fi
	fi
	echo "ninja_command ${ninja_command}"
}

function cp_compile_commands_json() {
	# vscode clangd 插件需要这个文件来作代码提示
	cp ./cmake-build-debug/compile_commands.json .
}

function clean() {
	"${cmake_command}" --build cmake-build-debug --target clean
}

function compile() {
	# 编译
	local target="${1}"
	# shellcheck disable=SC2068
	"${cmake_command}" ${@:2} -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_BUILD_TYPE=Debug -DCMAKE_MAKE_PROGRAM="${ninja_command}" -G Ninja -S . -B cmake-build-debug
	"${cmake_command}" --build cmake-build-debug --target "${target}" -j 9
}

function test() {
	compile "cbdai"
	cp_compile_commands_json
	time ./cmake-build-debug/cbdai "$@"
	# 运行包含 Sanitizer 检查的测试
	compile "santest"
	./cmake-build-debug/santest "$@"
}

function test_atstr() {
	# 测试 atstr 实现
	compile "test_atstr"
	./cmake-build-debug/test_atstr
}

function repl() {
	# repl
	compile "repl"
	./cmake-build-debug/repl
}

function show_ast() {
	# 打印代码解析出来的 ast
	compile "show_ast"
	./cmake-build-debug/show_ast "$@"
}

function benchmark() {
	# benchmark 测试
	compile "benchmark"
	./cmake-build-debug/benchmark "$@"
	# gprof ./cmake-build-debug/benchmark gmon.out > benchmark.txt
}

function benchmark_profile() {
	# benchmark 测试性能分析
	compile "benchmark_profile"
	# compile "benchmark_profile" -DCMAKE_C_COMPILER="${llvm_binpath}/clang"
	./cmake-build-debug/benchmark_profile "$@"
	gprof ./cmake-build-debug/benchmark_profile gmon.out >benchmark.txt
	# "${llvm_binpath}/llvm-profdata" merge *.profraw -output=code.profdata
	# "${llvm_binpath}/llvm-profdata" show code.profdata > zzz_benchmark.txt
	# rm *.profraw code.profdata
}

function mem() {
	# 内存检查
	#    似乎在 Mac 上不行
	#    leaks 执行之后会卡在那里，下面是程序的输出
	#cbdai(45301) MallocStackLogging: could not tag MSL-related memory as no_footprint, so those pages will be included in process footprint - (null)
	#cbdai(45301) MallocStackLogging: recording malloc and VM allocation stacks using lite mode
	#Running test suite with seed 0x38c8df50...
	#/dai/tokenize/test_next_token        cbdai(45302) MallocStackLogging: turning off stack logging (had been recording malloc and VM allocation stacks using lite mode)
	#    leaks -atExit -- ~/CLionProjects/cbdai/cmake-build-debug/cbdai
	compile "mem"
	valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all --trace-children=yes ./cmake-build-debug/mem --no-fork
}

function memrepl() {
	# 带内存检查的 repl
	compile "repl"
	valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all --trace-children=yes ./cmake-build-debug/repl
}

function coverage() {
	# 输出测试覆盖率
	compile "coverage" -DCMAKE_C_COMPILER="${llvm_binpath}/clang"
	LLVM_PROFILE_FILE="coverage.profraw" ./cmake-build-debug/coverage --no-fork
	"${llvm_binpath}/llvm-profdata" merge -sparse coverage.profraw -o coverage.profdata
	# "${llvm_binpath}/llvm-cov" export -format=text -instr-profile=coverage.profdata ./cmake-build-debug/coverage >coverage.json
	# llvm-coverage-viewer -j coverage.json -o coverage.html
	if [ "$#" -gt 0 ]; then
		"${llvm_binpath}/llvm-cov" show -show-expansions -show-branches=count -instr-profile=coverage.profdata ./cmake-build-debug/coverage "$@" >coverage.txt
	else
		"${llvm_binpath}/llvm-cov" report ./cmake-build-debug/coverage -instr-profile=coverage.profdata >coverage.txt
	#   python3 cut_coverage_output.py coverage.txt
	fi
}

function debug() {
	# 在命令行开启 debug 程序
	compile "debug"
	case "${os_name}" in
	darwin*)
		lldb -- ./cmake-build-debug/debug --no-fork
		;;
	linux*)
		gdb --args ./cmake-build-debug/debug --no-fork
		;;
	*)
		echo "not support debug in ${os_name}"
		;;
	esac
}

function fmt() {
	# 格式化代码
	# check clang-format exist
	if ! command -v clang-format &>/dev/null; then
		echo "clang-format could not be found"
		exit 1
	fi
	clang_format_command=$(command -v clang-format)
	"${clang_format_command}" -i --verbose ./*.c
	"${clang_format_command}" -i --verbose atstr/*.c
	"${clang_format_command}" -i --verbose test/*.c test/*.h
	"${clang_format_command}" -i --verbose src/*.c src/dai_*.h
	"${clang_format_command}" -i --verbose src/dai_ast/*.c src/dai_ast/*.h
}

function runfile() {
	compile "runfile"
	./cmake-build-debug/runfile "$@"
}

find_os_name
find_cmake
find_ninja
"$@"
