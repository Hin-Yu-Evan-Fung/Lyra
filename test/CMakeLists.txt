include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.15.2
)
FetchContent_MakeAvailable(googletest) # This line is mandatory to actually load GTest

file(GLOB GTEST_SRC "*.cpp")
add_executable(LyraTests ${GTEST_SRC})
target_link_libraries(LyraTests PRIVATE LyraLib GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(LyraTests)
