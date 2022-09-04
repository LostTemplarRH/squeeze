cmake_minimum_required(VERSION 3.15)
project(lzss LANGUAGES CXX)

option(SQUEEZE_BUILD_BENCHMARKS "Build benchmarks" OFF)
option(SQUEEZE_BUILD_PYTHON_PACKAGE "Build Python package" OFF)
option(SQUEEZE_BUILD_CLI "Build examples CLI" ON)

add_library(squeeze INTERFACE)
target_sources(squeeze
  INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/squeeze.h
)
target_include_directories(squeeze INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(squeeze INTERFACE cxx_std_20)

add_subdirectory(examples)

if (SQUEEZE_BUILD_BENCHMARKS)
  add_subdirectory(benchmark)
endif()

if (SQUEEZE_BUILD_PYTHON_PACKAGE)
  add_subdirectory(python)
endif()