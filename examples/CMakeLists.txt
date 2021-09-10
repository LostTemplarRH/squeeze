add_library(squeeze-namco
  namco/Lz80.h
  namco/Lz80.cc
)
target_link_libraries(squeeze-namco PUBLIC squeeze)
target_include_directories(squeeze-namco PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(squeeze-cli
  Main.cc
  CLI11.h
)
target_link_libraries(squeeze-cli
  PRIVATE
    squeeze-namco
)