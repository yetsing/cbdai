cmake_minimum_required(VERSION 3.22)
project(cbdai C)

set(CMAKE_C_STANDARD 11)

include_directories(.)
include_directories(src)
include_directories(src/utf8.h)
include_directories(src/hashmap.c)
# include_directories(src/utils)
include_directories(test)

add_compile_options(-g -Wno-unused-function -Werror -Wall)

file(GLOB_RECURSE MAIN_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)
file(GLOB_RECURSE TEST_SRC ${CMAKE_CURRENT_SOURCE_DIR}/test/*.c)

add_executable(cbdai main.c ${MAIN_SRC} ${TEST_SRC} munit/munit.c)
add_executable(cbdai-debug main.c ${MAIN_SRC} ${TEST_SRC} munit/munit.c)  # for clion debug
add_executable(santest main.c ${MAIN_SRC} ${TEST_SRC} munit/munit.c)
add_executable(mem main.c ${MAIN_SRC} ${TEST_SRC} munit/munit.c)
add_executable(coverage main.c ${MAIN_SRC} ${TEST_SRC} munit/munit.c)
add_executable(debug main.c ${MAIN_SRC} ${TEST_SRC} munit/munit.c)  # for cli debug
target_link_libraries(cbdai m)
target_link_libraries(cbdai-debug m)
target_link_libraries(santest m)
target_link_libraries(mem m)
target_link_libraries(coverage m)
target_link_libraries(debug m)

add_executable(test_atstr atstr/test_atstr.c atstr/atstr.c munit/munit.c)

# Step 1: Add cache variables with defaults
option(DAI_TEST_VERBOSE "Enable verbose testing" OFF)
# Step 2: Update target definitions conditionally
if(DAI_TEST_VERBOSE)
    target_compile_definitions(cbdai PRIVATE DAI_TEST_VERBOSE)
endif()
option(DAI_DEBUG_GC "Enable debug gc" OFF)
# Step 2: Update target definitions conditionally
if(DAI_DEBUG_GC)
    target_compile_definitions(cbdai PRIVATE DEBUG_STRESS_GC)
endif()


target_compile_definitions(cbdai PRIVATE DAI_TEST)
target_compile_definitions(cbdai-debug PRIVATE DAI_TEST)
target_compile_definitions(santest PRIVATE DAI_TEST)
target_compile_definitions(santest PRIVATE DAI_SANTEST_OUTPUT)
target_compile_definitions(mem PRIVATE DAI_TEST)

target_compile_options(coverage PRIVATE -fprofile-instr-generate -fcoverage-mapping)
target_link_options(coverage PRIVATE -fprofile-instr-generate)
target_link_libraries(coverage m)

target_compile_options(santest PRIVATE -fsanitize=address)
target_link_options(santest PRIVATE -fsanitize=address)

add_executable(repl repl.c ${MAIN_SRC} atstr/atstr.c)
target_link_libraries(repl m)
add_executable(show_ast show_ast.c ${MAIN_SRC})
target_link_libraries(show_ast m)

add_executable(benchmark_profile benchmark.c ${MAIN_SRC})
target_compile_options(benchmark_profile PRIVATE -pg)
target_link_options(benchmark_profile PRIVATE -pg)
# target_compile_options(benchmark_profile PRIVATE -fprofile-instr-generate)
# target_link_options(benchmark_profile PRIVATE -fprofile-instr-generate)
target_compile_definitions(benchmark_profile PRIVATE BENCHMARK_PROFILE)
target_link_libraries(benchmark_profile m)

add_executable(benchmark benchmark.c ${MAIN_SRC})
target_compile_options(benchmark PRIVATE -O3 -flto)
target_compile_definitions(benchmark PRIVATE BENCHMARK)
target_link_libraries(benchmark m)

add_executable(runfile runfile.c ${MAIN_SRC})
target_compile_options(runfile PRIVATE -O3 -flto)
target_link_libraries(runfile m)

add_executable(dis dis.c ${MAIN_SRC})
target_link_libraries(dis m)
