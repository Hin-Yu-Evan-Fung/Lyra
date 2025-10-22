cmake_minimum_required(VERSION 3.20)
project(Lyra LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(include)
set(
    SOURCES 
    src/main.cpp
    src/bitboard.cpp
    src/board.cpp
)

add_executable(Lyra ${SOURCES})

target_compile_options(Lyra PRIVATE
 -g -std=c++20
)
