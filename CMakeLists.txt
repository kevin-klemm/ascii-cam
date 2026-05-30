cmake_minimum_required(VERSION 3.16)
project(AsciiEncodeRender CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(WITH_OPENCV "Build the OpenCV capture backend (webcam/RTSP)" ON)
option(BUILD_TESTS "Build the unit test suite" ON)

# Native SIMD (NEON on Apple Silicon/ARM, SSE2/AVX on x86) where supported.
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-march=native" HAS_MARCH_NATIVE)
add_compile_options(-O3)
if(HAS_MARCH_NATIVE)
  add_compile_options(-march=native)
endif()

find_package(Threads REQUIRED)

# ----------------------------------------------------------------------------
# Main application
# ----------------------------------------------------------------------------
add_executable(asciicam src/main.cpp)
target_include_directories(asciicam PRIVATE src)
target_link_libraries(asciicam PRIVATE Threads::Threads)

if(WITH_OPENCV)
  find_package(OpenCV QUIET COMPONENTS core videoio)
  if(OpenCV_FOUND)
    message(STATUS "OpenCV found (${OpenCV_VERSION}) - webcam/RTSP backend enabled")
    target_compile_definitions(asciicam PRIVATE WITH_OPENCV)
    target_include_directories(asciicam PRIVATE ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(asciicam PRIVATE ${OpenCV_LIBS})
  else()
    message(WARNING "OpenCV not found - building IoT mode (V4L2 + pipe sources only)")
  endif()
endif()

# ----------------------------------------------------------------------------
# Offline demo: renders a synthetic animated scene (no camera/OpenCV needed)
# ----------------------------------------------------------------------------
add_executable(demo demo/demo.cpp)
target_include_directories(demo PRIVATE src)

# Micro-benchmark: ms/frame (CPU ceiling) and bytes/frame (SSH cost).
add_executable(bench bench/bench.cpp)
target_include_directories(bench PRIVATE src)

# ----------------------------------------------------------------------------
# Unit tests (no OpenCV, no hardware required)
# ----------------------------------------------------------------------------
if(BUILD_TESTS)
  enable_testing()
  file(GLOB TEST_SOURCES CONFIGURE_DEPENDS test/test_*.cpp)
  add_executable(unit_tests ${TEST_SOURCES})
  target_include_directories(unit_tests PRIVATE src test)
  target_link_libraries(unit_tests PRIVATE Threads::Threads)
  add_test(NAME unit_tests COMMAND unit_tests)
endif()
