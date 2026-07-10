cmake_minimum_required(VERSION 3.22)

set(CMAKE_ASM_NASM_OBJECT_FORMAT elf32)
project(drweb_problems LANGUAGES C CXX ASM_NASM)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(DRWEB_WARNINGS -Wall -Wextra -Wshadow -Wconversion -pedantic-errors)

if(DRWEB_SANITIZE)
    set(DRWEB_SANITIZE_FLAGS -fsanitize=${DRWEB_SANITIZE} -g -fno-omit-frame-pointer)
endif()

add_library(problem1 OBJECT problem1.cpp)
target_compile_options(problem1 PRIVATE ${DRWEB_WARNINGS} -fno-exceptions -fno-rtti)

add_library(problem2 OBJECT problem2.cpp)
target_compile_options(problem2 PRIVATE ${DRWEB_WARNINGS} -fno-exceptions -fno-rtti)

add_library(problem3 STATIC problem3.c problem3.asm)
target_compile_options(problem3 PRIVATE "$<$<COMPILE_LANGUAGE:C>:${DRWEB_WARNINGS};-m32>")

enable_testing()

add_library(catch2 STATIC third_party/catch2/catch_amalgamated.cpp)
target_include_directories(catch2 SYSTEM PUBLIC third_party/catch2)

add_library(catch2_m32 STATIC third_party/catch2/catch_amalgamated.cpp)
target_include_directories(catch2_m32 SYSTEM PUBLIC third_party/catch2)
target_compile_options(catch2_m32 PRIVATE -m32)

function(drweb_add_test name)
    add_executable(${name} tests/${name}.cpp)
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_options(${name} PRIVATE ${DRWEB_WARNINGS} ${DRWEB_SANITIZE_FLAGS})
    target_link_libraries(${name} PRIVATE catch2)
    if(DRWEB_SANITIZE)
        target_link_options(${name} PRIVATE ${DRWEB_SANITIZE_FLAGS})
    endif()
    add_test(NAME ${name} COMMAND ${name})
endfunction()

drweb_add_test(test_problem1)
drweb_add_test(test_problem2)

add_executable(test_problem3 tests/test_problem3.cpp)
target_include_directories(test_problem3 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(test_problem3 PRIVATE ${DRWEB_WARNINGS} -m32)
target_link_options(test_problem3 PRIVATE -m32)
target_link_libraries(test_problem3 PRIVATE catch2_m32 problem3)
add_test(NAME test_problem3 COMMAND test_problem3)
