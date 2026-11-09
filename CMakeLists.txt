cmake_minimum_required(VERSION 3.14)
project(labwork3_sandpile_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(LABWORK3_BIN "" CACHE FILEPATH "Path to your labwork3 executable")
if(NOT LABWORK3_BIN)
  message(FATAL_ERROR "Pass the path to your program: cmake -B build -DLABWORK3_BIN=/abs/path/to/labwork3")
endif()
get_filename_component(LABWORK3_BIN_ABS "${LABWORK3_BIN}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(sandpile_tests
  src/bmp_reader.cpp
  src/runner.cpp
  src/spec_test.cpp
  src/extra_test.cpp
)
target_link_libraries(sandpile_tests PRIVATE GTest::gtest_main)
target_compile_definitions(sandpile_tests PRIVATE
  LABWORK3_BIN="${LABWORK3_BIN_ABS}"
  DATA_DIR="${CMAKE_SOURCE_DIR}/data"
)

enable_testing()
include(GoogleTest)
gtest_discover_tests(sandpile_tests DISCOVERY_TIMEOUT 30 PROPERTIES TIMEOUT 600)
