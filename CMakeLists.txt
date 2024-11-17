cmake_minimum_required(VERSION 3.22)
project(cbdai C)

set(CMAKE_C_STANDARD 11)

include_directories(.)
include_directories(src)
# include_directories(src/utils)
include_directories(test)

add_compile_options(-g -Werror -Wall)

file(GLOB_RECURSE MAIN_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)
file(GLOB_RECURSE TEST_SRC ${CMAKE_CURRENT_SOURCE_DIR}/test/*.c)

add_executable(cbdai main.c ${MAIN_SRC} ${TEST_SRC} munit/munit.c)
add_executable(santest main.c ${MAIN_SRC} ${TEST_SRC} munit/munit.c)
add_executable(mem main.c ${MAIN_SRC} ${TEST_SRC} munit/munit.c)
add_executable(coverage main.c ${MAIN_SRC} ${TEST_SRC} munit/munit.c)
add_executable(debug main.c ${MAIN_SRC} ${TEST_SRC} munit/munit.c)

add_executable(test_atstr atstr/test_atstr.c atstr/atstr.c munit/munit.c)

target_compile_definitions(cbdai PRIVATE TEST)
target_compile_definitions(santest PRIVATE TEST)
target_compile_definitions(santest PRIVATE SANTEST_OUTPUT)
target_compile_definitions(mem PRIVATE TEST)

target_compile_options(coverage PRIVATE -fprofile-instr-generate -fcoverage-mapping)
target_link_options(coverage PRIVATE -fprofile-instr-generate)

target_compile_options(santest PRIVATE -fsanitize=address)
target_link_options(santest PRIVATE -fsanitize=address)

add_executable(repl repl.c ${MAIN_SRC} atstr/atstr.c)
add_executable(show_ast show_ast.c ${MAIN_SRC})

add_executable(benchmark_profile benchmark.c ${MAIN_SRC})
target_compile_options(benchmark_profile PRIVATE -pg)
target_link_options(benchmark_profile PRIVATE -pg)
# target_compile_options(benchmark_profile PRIVATE -fprofile-instr-generate)
# target_link_options(benchmark_profile PRIVATE -fprofile-instr-generate)
target_compile_definitions(benchmark_profile PRIVATE BENCHMARK_PROFILE)

add_executable(benchmark benchmark.c ${MAIN_SRC})
target_compile_definitions(benchmark PRIVATE BENCHMARK)

add_executable(runfile runfile.c ${MAIN_SRC})
