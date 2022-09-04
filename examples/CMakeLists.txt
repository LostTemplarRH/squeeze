add_library(squeeze-namco
  namco/Lz80.h
  namco/Lz80.cc
  namco/Lz0103.h
  namco/Lz0103.cc
  namco/Lz0103Data.h
)
target_link_libraries(squeeze-namco PUBLIC squeeze)
target_include_directories(squeeze-namco PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if (SQUEEZE_BUILD_CLI)
  add_executable(squeeze-cli
    Main.cc
    CLI11.h
  )
  target_link_libraries(squeeze-cli
    PRIVATE
      squeeze-namco
  )
endif()
