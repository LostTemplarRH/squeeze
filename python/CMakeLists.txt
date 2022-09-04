cmake_minimum_required(VERSION 3.21)
project(pysqueeze LANGUAGES CXX)

if(PROJECT_IS_TOP_LEVEL)
  set(SQUEEZE_BUILD_CLI OFF CACHE INTERNAL "Don't build examples CLI")
  add_subdirectory(../ squeeze)
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/get_cpm.cmake)
CPMAddPackage("gh:pybind/pybind11@2.10.0")

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

if(PROJECT_IS_TOP_LEVEL)
  install(TARGETS _squeeze DESTINATION squeeze/)
else()
  add_custom_command(TARGET _squeeze PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE:_squeeze> "${CMAKE_CURRENT_SOURCE_DIR}/squeeze/$<TARGET_FILE_NAME:_squeeze>"
    COMMENT "mklink ${src} -> ${dest}"
  )
endif()
