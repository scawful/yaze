include(FetchContent)

FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/ftxui
  GIT_TAG v5.0.0
)

FetchContent_GetProperties(ftxui)
if(NOT ftxui_POPULATED)
  FetchContent_Populate(ftxui)
  add_subdirectory(${ftxui_SOURCE_DIR} ${ftxui_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

add_executable(
  z3ed
  cli/cli_main.cc
  cli/tui.cc
  cli/handlers/compress.cc
  cli/handlers/patch.cc
  cli/handlers/tile16_transfer.cc
  app/rom.cc
  app/core/project.cc
  app/core/asar_wrapper.cc
  app/core/platform/file_dialog.mm
  app/core/platform/file_dialog.cc
  ${YAZE_APP_EMU_SRC}
  ${YAZE_APP_GFX_SRC}
  ${YAZE_APP_ZELDA3_SRC}
  ${YAZE_UTIL_SRC}
  ${YAZE_GUI_SRC}
  ${IMGUI_SRC}
)

target_include_directories(
  z3ed PUBLIC
  lib/
  app/
  ${ASAR_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/incl/
  ${CMAKE_SOURCE_DIR}/src/
  ${PNG_INCLUDE_DIRS}
  ${SDL2_INCLUDE_DIR}
  ${GLEW_INCLUDE_DIRS}
  ${PROJECT_BINARY_DIR}
)

target_link_libraries(
  z3ed PUBLIC
  asar-static
  ftxui::component
  ftxui::screen
  ftxui::dom
  absl::flags
  absl::flags_parse
  ${ABSL_TARGETS}
  ${SDL_TARGETS}
  ${PNG_LIBRARIES}
  ${GLEW_LIBRARIES}
  ${OPENGL_LIBRARIES}
  ${CMAKE_DL_LIBS}
  ImGuiTestEngine
  ImGui
)
