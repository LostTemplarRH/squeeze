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
install(TARGETS _squeeze DESTINATION .)