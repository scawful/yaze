# CMake Specifications --------------------------------------------------------
cmake_minimum_required(VERSION 3.10)

# Yet Another Zelda3 Editor
# by scawful
project(yaze VERSION 0.01)

# C++ Standard Specifications -------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_MODULE_LINKER_FLAGS \"-Wl,--no-undefined -Wl,--no-undefined\")
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
set(BUILD_SHARED_LIBS ON)

# Abseil Standard Specifications ----------------------------------------------
include(cmake/absl.cmake)
add_subdirectory(src/lib/abseil-cpp)

include(cmake/openssl.cmake)
set(Protobuf_PROTOC_EXECUTABLE "/Users/scawful/code/protobuf/bin/protoc")
set(Protobuf_INCLUDE_DIRS "/Users/scawful/code/protobuf/include/")
set(PROTOBUF_INCLUDE_PATH ${CMAKE_CURRENT_BINARY_DIR}
    CACHE INTERNAL "Path to generated protobuf files.")
include_directories(${PROTOBUF_INCLUDE_PATH})

# Video Libraries -------------------------------------------------------------
find_package(PNG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)

# SDL2 ------------------------------------------------------------------------
if (UNIX)
  add_subdirectory(src/lib/SDL)
else()
  find_package(SDL2)
endif()
set(SDL2MIXER_OPUS OFF)
set(SDL2MIXER_FLAC OFF)
set(SDL2MIXER_MOD OFF)
set(SDL2MIXER_MIDI_FLUIDSYNTH OFF)
find_library(SDL_MIXER_LIBRARY
  NAMES SDL_mixer
  HINTS
    ENV SDLMIXERDIR
    ENV SDLDIR
  PATH_SUFFIXES lib ${VC_LIB_PATH_SUFFIX}
  )
add_subdirectory(src/lib/SDL_mixer)

# Asar ------------------------------------------------------------------------
add_subdirectory(src/lib/asar/src)
include(cmake/asar.cmake)

# snes-spc --------------------------------------------------------------------
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
ADD_LIBRARY(snes_spc STATIC ${SNES_SPC_SOURCES} src/app/spc700/spc700.def)

# ImGui -----------------------------------------------------------------------
include(cmake/imgui.cmake)

# Project Files
add_subdirectory(src)
add_subdirectory(test)