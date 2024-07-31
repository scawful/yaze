add_executable(
  z3ed
  cli/z3ed.cc
  cli/patch.cc
  cli/command_handler.cc
  app/rom.cc
  app/core/common.cc
  app/core/labeling.cc
  app/gui/pipeline.cc
  app/editor/context/gfx_context.cc
  app/core/platform/file_dialog.mm
  ${YAZE_APP_EMU_SRC}
  ${YAZE_APP_GFX_SRC}
  ${YAZE_APP_ZELDA3_SRC}
  ${YAZE_GUI_SRC}
  ${IMGUI_SRC}
)

target_include_directories(
  z3ed PUBLIC
  lib/
  app/
  ${CMAKE_SOURCE_DIR}/src/
  ${PNG_INCLUDE_DIRS}
  ${SDL2_INCLUDE_DIR}
  ${GLEW_INCLUDE_DIRS}
)

target_link_libraries(
  z3ed PUBLIC
  ${ABSL_TARGETS}
  ${SDL_TARGETS}
  ${PNG_LIBRARIES}
  ${GLEW_LIBRARIES}
  ${OPENGL_LIBRARIES}
  ${CMAKE_DL_LIBS}
  ImGui
)