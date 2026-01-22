cmake_minimum_required(VERSION 3.20)
project(Lyra LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O3 -DUSE_PEXT -Wall -flto -fno-rtti -msse2 -msse3 -msse4 -msse4.1 -mpopcnt -mavx2 -mbmi -mbmi2 -mmmx -funroll-loops -finline -fomit-frame-pointer")

include_directories(include)
set(
    SOURCES 
    src/main.cpp
    src/bitboard.cpp
    src/board.cpp
    src/move.cpp
    src/thread.cpp
    src/movepick.cpp
    src/clock.cpp
    src/eval.cpp
    src/search.cpp
    src/engine.cpp
    src/uci.cpp
)

add_library(LyraLib STATIC ${SOURCES})
add_executable(Lyra src/main.cpp)
target_link_libraries(Lyra PRIVATE LyraLib)

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.15.2
)

enable_testing()

file(GLOB GTEST_SRC "tests/*.cpp")
add_executable(LyraTests ${GTEST_SRC})
target_link_libraries(LyraTests PRIVATE LyraLib gtest_main)

include(GoogleTest)
gtest_discover_tests(LyraTests)
