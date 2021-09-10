cmake_minimum_required(VERSION 3.15)
project(lzss LANGUAGES CXX)

add_library(squeeze INTERFACE)
target_sources(squeeze
  INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/squeeze.h
)
target_include_directories(squeeze INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(squeeze INTERFACE cxx_std_20)

add_subdirectory(examples)