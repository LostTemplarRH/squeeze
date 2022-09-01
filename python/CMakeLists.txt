cmake_minimum_required(VERSION 3.21)
project(pysqueeze LANGUAGES CXX)

if(PROJECT_IS_TOP_LEVEL)
  add_subdirectory(../ squeeze)
endif()

add_subdirectory(pybind11)

pybind11_add_module(_squeeze
  squeeze.cc
)
target_link_libraries(_squeeze
  PRIVATE
    squeeze
    squeeze-namco
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set_property(TARGET squeeze-namco PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()

install(TARGETS _squeeze DESTINATION .)