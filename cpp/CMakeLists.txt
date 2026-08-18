cmake_minimum_required(VERSION 3.20)
project(smt2048 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    # /utf-8 обязателен: в исходниках кириллица в строковых литералах.
    add_compile_options(/utf-8 /W4 /permissive-)
endif()

add_library(smt_core STATIC
    src/bitboard.cpp
    src/solver.cpp
    src/capture.cpp
    src/board.cpp
    src/overlay.cpp
)
target_include_directories(smt_core PUBLIC src)
target_link_libraries(smt_core PUBLIC gdiplus)

add_executable(smt WIN32 src/main.cpp)
target_link_libraries(smt PRIVATE smt_core)

enable_testing()
add_executable(smt_tests tests/test_solver.cpp)
target_link_libraries(smt_tests PRIVATE smt_core)
add_test(NAME solver COMMAND smt_tests "${CMAKE_CURRENT_SOURCE_DIR}/../testdata")
