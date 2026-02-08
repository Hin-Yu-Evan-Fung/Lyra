cmake_minimum_required(VERSION 3.20)
project(Lyra LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_PEXT -Wall -flto -fno-rtti -msse2 -msse3 -msse4 -msse4.1 -mpopcnt -mavx2 -mbmi -mbmi2 -mmmx -funroll-loops -finline -fomit-frame-pointer")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -O0 -g")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG")

include_directories(include)
set(
    SOURCES 
    src/main.cpp
    src/board/board.cpp
    src/board/move.cpp
    src/core/bitboard.cpp
    src/engine/clock.cpp
    src/engine/engine.cpp
    src/engine/thread.cpp
    src/engine/uci.cpp
    src/search/eval.cpp
    src/search/movepick.cpp
    src/search/search.cpp
    src/search/utils.cpp
    src/utils/tt.cpp
)

add_library(LyraLib STATIC ${SOURCES})
add_executable(Lyra src/main.cpp)
target_link_libraries(Lyra PRIVATE LyraLib)


# Google Tests

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.15.2
)
FetchContent_MakeAvailable(googletest) # This line is mandatory to actually load GTest

enable_testing()

file(GLOB GTEST_SRC "tests/*.cpp")
add_executable(LyraTests ${GTEST_SRC})
target_link_libraries(LyraTests PRIVATE LyraLib GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(LyraTests)
