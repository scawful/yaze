# CMake Specifications 
cmake_minimum_required(VERSION 3.10)

# Yet Another Zelda3 Editor
# by scawful
project(yaze VERSION 0.01)

# C++ Standard Specifications 
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_MODULE_LINKER_FLAGS \"-Wl,--no-undefined -Wl,--no-undefined\")
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
set(BUILD_SHARED_LIBS ON)

# Abseil Standard Specifications
include(cmake/absl.cmake)
add_subdirectory(src/lib/abseil-cpp)

include(cmake/openssl.cmake)

# Video Libraries 
find_package(PNG REQUIRED)
find_package(OpenGL REQUIRED)
include(cmake/sdl2.cmake)

# Asar 
# add_subdirectory(src/lib/asar/src)
# include(cmake/asar.cmake)

# snes-spc
ADD_DEFINITIONS(-DSNES_SPC_EXPORTS)
set(SNES_SPC_SOURCES
  "../src/lib/snes_spc/snes_spc/spc.cpp"
  "../src/lib/snes_spc/snes_spc/SNES_SPC.cpp"
  "../src/lib/snes_spc/snes_spc/SNES_SPC_misc.cpp"
  "../src/lib/snes_spc/snes_spc/SNES_SPC_state.cpp"
  "../src/lib/snes_spc/snes_spc/SPC_DSP.cpp"
  "../src/lib/snes_spc/snes_spc/dsp.cpp"
  "../src/lib/snes_spc/snes_spc/SPC_Filter.cpp"
  "../src/lib/snes_spc/demo/wave_writer.c"
  "../src/lib/snes_spc/demo/demo_util.c"
)
include_directories(src/lib/snes_spc/snes_spc)
ADD_LIBRARY(snes_spc STATIC ${SNES_SPC_SOURCES} src/app/zelda3/music/spc700.def)

# ImGui
include(cmake/imgui.cmake)

# Project Files
add_subdirectory(src)
add_subdirectory(test)