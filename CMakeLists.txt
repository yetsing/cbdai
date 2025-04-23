cmake_minimum_required(VERSION 3.22)
project(cbdai C)

set(CMAKE_C_STANDARD 11)

include_directories(.)
include_directories(src)
include_directories(src/utf8.h)
include_directories(src/hashmap.c)

add_compile_options(-Wno-unused-function -Werror -Wall)

# 保存原全局设置
#set(original_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS})
# 强制第三方库编译为静态库
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lib/cwalk)
# 恢复全局设置
#set(BUILD_SHARED_LIBS ${original_BUILD_SHARED_LIBS})

file(GLOB_RECURSE MAIN_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)
file(GLOB_RECURSE DAI_SRC ${CMAKE_CURRENT_SOURCE_DIR}/dai/*.c)
file(GLOB_RECURSE CBDAI_SRC ${CMAKE_CURRENT_SOURCE_DIR}/cbdai/*.c)
file(GLOB_RECURSE TEST_SRC ${CMAKE_CURRENT_SOURCE_DIR}/test/*.c)

add_executable(test ${MAIN_SRC} ${CBDAI_SRC} ${TEST_SRC} munit/munit.c)
target_include_directories(test PRIVATE test cbdai)
target_link_libraries(test PRIVATE m cwalk)
add_executable(test-debug ${MAIN_SRC} ${CBDAI_SRC} ${TEST_SRC} munit/munit.c) # for clion debug
target_include_directories(test-debug PRIVATE test cbdai)
target_link_libraries(test-debug PRIVATE m cwalk)
target_compile_definitions(test PRIVATE DAI_TEST)
target_compile_definitions(test-debug PRIVATE DAI_TEST)

option(DAI_TEST_VERBOSE "Enable verbose testing" OFF)
if(DAI_TEST_VERBOSE)
    target_compile_definitions(test PRIVATE DAI_TEST_VERBOSE)
endif()
option(DAI_DEBUG_GC "Enable debug gc" OFF)
if(DAI_DEBUG_GC)
    target_compile_definitions(test PRIVATE DEBUG_STRESS_GC)
endif()


add_executable(santest ${MAIN_SRC} ${CBDAI_SRC} ${TEST_SRC} munit/munit.c)
target_include_directories(santest PRIVATE test cbdai)
target_link_libraries(santest PRIVATE m cwalk)
target_compile_definitions(santest PRIVATE DAI_TEST)
target_compile_definitions(santest PRIVATE DAI_SANTEST_OUTPUT)
target_compile_options(santest PRIVATE -fsanitize=address)
target_link_options(santest PRIVATE -fsanitize=address)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lib/SDL EXCLUDE_FROM_ALL)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/lib/SDL_image EXCLUDE_FROM_ALL)

# set the output directory for built objects.
# This makes sure that the dynamic library goes into the build directory automatically.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

# Ensure the dai target is properly configured
add_executable(dai ${MAIN_SRC} ${DAI_SRC})
if(WIN32)
    target_compile_options(dai PRIVATE -O3)
else()
    target_compile_options(dai PRIVATE -O3 -flto)
    endif()
target_link_libraries(dai PRIVATE m cwalk SDL3_image::SDL3_image SDL3::SDL3)

if(WIN32)
    if(MSVC)
        target_link_libraries(test PRIVATE bcrypt.lib)
        target_link_libraries(test-debug PRIVATE bcrypt.lib)
        target_link_libraries(santest PRIVATE bcrypt.lib)
        target_link_libraries(dai PRIVATE bcrypt.lib)
    else()
        target_link_libraries(test PRIVATE bcrypt)
        target_link_libraries(test-debug PRIVATE bcrypt)
        target_link_libraries(santest PRIVATE bcrypt)
        target_link_libraries(dai PRIVATE bcrypt)
    endif()
endif()

