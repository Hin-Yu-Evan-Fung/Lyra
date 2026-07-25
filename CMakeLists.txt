cmake_minimum_required(VERSION 3.20)
project(Lyra LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build)

add_compile_options(-Wall -funroll-loops -finline -fomit-frame-pointer -O3)
add_compile_definitions(TUNE)

if (EMSCRIPTEN)
  add_compile_options(-g -msimd128 -mrelaxed-simd)
  add_link_options(-gsource-map)
else()
  add_compile_definitions(USE_PEXT)
  add_compile_options(
    -g -flto -msse2 -msse3 -msse4 -msse4.1
    -mpopcnt -mavx2 -mbmi -mbmi2 -mmmx
  )
endif()

file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS "src/core/*.cpp")
file(GLOB_RECURSE NNUE_SOURCES CONFIGURE_DEPENDS "src/nnue/*.cpp")
add_library(LyraLib STATIC ${CORE_SOURCES} ${NNUE_SOURCES})
include_directories(src/core src/nnue)

if(EMSCRIPTEN)
  file(GLOB_RECURSE WASM_SOURCES CONFIGURE_DEPENDS "src/wasm/*.cpp")
  include_directories(src/wasm)
  add_executable(LyraWasm ${WASM_SOURCES})
  target_link_libraries(LyraWasm PRIVATE LyraLib embind)
  target_link_options(LyraWasm PRIVATE
    "-lembind"
    "-sEXPORT_ES6=1"
    "-sMODULARIZE=1"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sEXPORTED_RUNTIME_METHODS=['ccall']"
    "-sASSERTIONS=1"
    "-sSTACK_OVERFLOW_CHECK=2"
    "-sNO_DISABLE_EXCEPTION_CATCHING"
    "-sASYNCIFY"
    "-sASYNCIFY_STACK_SIZE=65536"
    "-sEXPORTED_RUNTIME_METHODS=['HEAP8']"
    "SHELL: --embed-file ${CMAKE_SOURCE_DIR}/src/weights/vega.nnue@/vega.nnue"
  )
  target_compile_definitions(LyraWasm PRIVATE NETWORK_PATH="/vega.nnue")
else()
  file(GLOB_RECURSE CLI_SOURCES CONFIGURE_DEPENDS "src/cli/*.cpp")
  include_directories(src/cli)
  add_executable(Lyra ${CLI_SOURCES})
  target_link_libraries(Lyra PRIVATE LyraLib)
  target_compile_definitions(Lyra PRIVATE NETWORK_PATH="${CMAKE_SOURCE_DIR}/src/weights/vega.nnue")
  enable_testing()
  add_subdirectory(test)
endif()
