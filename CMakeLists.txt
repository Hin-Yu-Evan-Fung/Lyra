cmake_minimum_required(VERSION 3.20)
project(Lyra LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_PEXT -DTUNE -Wall -flto -fno-rtti -msse2 -msse3 -msse4 -msse4.1 -mpopcnt -mavx2 -mbmi -mbmi2 -mmmx -funroll-loops -finline -fomit-frame-pointer")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -O3 -g -fsanitize=undefined,address")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O3 -g")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build)

include_directories(src/cli src/core src/nnue)

file (GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS "src/core/*.cpp")
file (GLOB_RECURSE NNUE_SOURCES CONFIGURE_DEPENDS "src/nnue/*.cpp")
file (GLOB_RECURSE CLI_SOURCES CONFIGURE_DEPENDS "src/cli/*.cpp")

add_library(LyraLib STATIC ${CORE_SOURCES} ${NNUE_SOURCES})
include_directories(LyraLib PUBLIC src/core src/nnue)
target_compile_definitions(LyraLib PRIVATE NETWORK_PATH="${CMAKE_SOURCE_DIR}/src/weights/big.nnue")

add_executable(Lyra ${CLI_SOURCES})
target_link_libraries(Lyra PRIVATE LyraLib)

add_subdirectory(test)
