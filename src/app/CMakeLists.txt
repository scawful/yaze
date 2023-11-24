add_executable(
  yaze
  app/yaze.cc
  app/rom.cc
  app/emu/emulator.cc
  app/emu/audio/apu.cc
  app/emu/audio/spc700.cc
  app/emu/audio/dsp.cc
  app/emu/video/ppu.cc
  app/emu/memory/dma.cc
  app/emu/cpu.cc
  app/emu/snes.cc
  ${YAZE_APP_CORE_SRC}
  ${YAZE_APP_EDITOR_SRC}
  ${YAZE_APP_GFX_SRC}
  ${YAZE_APP_ZELDA3_SRC}
  ${YAZE_GUI_SRC}
  ${IMGUI_SRC}
)

target_include_directories(
  yaze PUBLIC
  lib/
  app/
  ${CMAKE_SOURCE_DIR}/src/
  ${PNG_INCLUDE_DIRS}
  ${SDL2_INCLUDE_DIR}
  lib/SDL_mixer/include/
)

target_link_libraries(
  yaze PUBLIC
  ${ABSL_TARGETS}
  ${SDL_TARGETS}
  ${PNG_LIBRARIES}
  ${CMAKE_DL_LIBS}
  SDL2_mixer
  ImGui
)

if (APPLE)
  target_link_libraries(yaze PUBLIC ${COCOA_LIBRARY})
endif()