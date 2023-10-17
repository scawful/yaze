add_executable(
  yaze
  app/yaze.cc
  app/rom.cc
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
  # ${ASAR_STATIC_SRC}
  # ${SNES_SPC_SOURCES}
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
  # lib/asar/src/asar/
  # lib/snes_spc/snes_spc/
)

target_link_libraries(
  yaze PUBLIC
  ${ABSL_TARGETS}
  ${SDL_TARGETS}
  ${PNG_LIBRARIES}
  ${CMAKE_DL_LIBS}
  SDL2_mixer
  ImGui
  # asar-static
  # snes_spc
)