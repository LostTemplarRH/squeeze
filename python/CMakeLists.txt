cmake_minimum_required(VERSION 3.15)
project(pysqueeze LANGUAGES CXX)

add_subdirectory(../ squeeze)
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