#!/usr/bin/env bash

set -e
# set -x

cmake_command=$(command -v cmake)
ninja_command=$(command -v ninja)
llvm_binpath="/usr/bin"
os_name="unknown"

function find_os_name() {
	case "$OSTYPE" in
      solaris*) os_name="solaris" ;;
      darwin*)  os_name="darwin" ;; 
      linux*)   os_name="linux" ;;
      bsd*)     os_name="bsd" ;;
      msys*)    os_name="windows" ;;
      cygwin*)  os_name="windows" ;;
      *)        echo "unknown: $OSTYPE" ;;
    esac
}

function find_cmake() {
	case "${os_name}" in
	darwin*)
		# MacOS 用 CLion 自带的
		echo "Using CLion CMake in ${os_name}"
		cmake_command="$HOME/Applications/CLion.app/Contents/bin/cmake/mac/aarch64/bin/cmake"
		ninja_command="$HOME/Applications/CLion.app/Contents/bin/ninja/mac/aarch64/ninja"
		llvm_binpath="$(brew --prefix llvm)/bin"
		;;
	esac

	echo "cmake_command: ${cmake_command}"
	echo "ninja_command: ${ninja_command}"
	echo "llvm_binpath: ${llvm_binpath}"
}

function cp_compile_commands_json() {
	cp ./cmake-build-debug/compile_commands.json .
}

function clean() {
	"${cmake_command}" --build cmake-build-debug --target clean
}

function compile() {
	local target="${1}"
	local option1="${2}"
	"${cmake_command}" ${option1} -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DCMAKE_BUILD_TYPE=Debug -DCMAKE_MAKE_PROGRAM="${ninja_command}" -G Ninja -S . -B cmake-build-debug
	"${cmake_command}" --build cmake-build-debug --target "${target}" -j 9
}

function test() {
	compile "cbdai"
	cp_compile_commands_json
	time ./cmake-build-debug/cbdai
	# 运行包含 Sanitizer 检查的测试
	compile "santest"
	./cmake-build-debug/santest
}

function test_atstr() {
	compile "test_atstr"
	./cmake-build-debug/test_atstr
}

function repl() {
	compile "repl"
	./cmake-build-debug/repl
}

function show_ast() {
	compile "show_ast"
	./cmake-build-debug/show_ast "$@"
}

function benchmark() {
	compile "benchmark"
	./cmake-build-debug/benchmark "$@"
	# gprof ./cmake-build-debug/benchmark gmon.out > benchmark.txt
}

function mem() {
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
	compile "repl"
	valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all --trace-children=yes ./cmake-build-debug/repl
}

function coverage() {
	compile "coverage" -DCMAKE_C_COMPILER="${llvm_binpath}/clang"
	LLVM_PROFILE_FILE="coverage.profraw" ./cmake-build-debug/coverage --no-fork
	"${llvm_binpath}/llvm-profdata" merge -sparse coverage.profraw -o coverage.profdata
	# "${llvm_binpath}/llvm-cov" export -format=text -instr-profile=coverage.profdata ./cmake-build-debug/coverage >coverage.json
	# llvm-coverage-viewer -j coverage.json -o coverage.html
	if [ "$#" -gt 0 ]; then
	  "${llvm_binpath}/llvm-cov" show -show-expansions -show-branches=count -instr-profile=coverage.profdata ./cmake-build-debug/coverage "$@" > coverage.txt
	else
	  "${llvm_binpath}/llvm-cov" report ./cmake-build-debug/coverage -instr-profile=coverage.profdata > coverage.txt
	#   python3 cut_coverage_output.py coverage.txt
	fi
}

function debug() {
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

find_os_name
find_cmake
"$@"
