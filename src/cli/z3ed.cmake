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

# Platform-specific file dialog sources
if(APPLE)
  set(FILE_DIALOG_SRC 
    app/core/platform/file_dialog.cc   # Utility functions (all platforms)
    app/core/platform/file_dialog.mm   # macOS-specific dialogs
  )
else()
  set(FILE_DIALOG_SRC app/core/platform/file_dialog.cc)
endif()

add_executable(
  z3ed
  cli/cli_main.cc
  cli/tui.cc
  cli/handlers/compress.cc
  cli/handlers/patch.cc
  cli/handlers/tile16_transfer.cc
  cli/handlers/dungeon.cc
  cli/handlers/gfx.cc
  cli/handlers/palette.cc
  cli/handlers/rom.cc
  cli/handlers/overworld.cc
  cli/handlers/sprite.cc
  cli/tui/tui_component.h
  cli/tui/asar_patch.cc
  cli/tui/palette_editor.cc
  cli/tui/command_palette.cc
  cli/modern_cli.cc
  cli/handlers/command_palette.cc
  cli/handlers/project.cc
  cli/handlers/agent.cc
  cli/handlers/agent/common.cc
  cli/handlers/agent/general_commands.cc
  cli/handlers/agent/test_commands.cc
  cli/handlers/agent/gui_commands.cc
  cli/service/ai_service.cc
  cli/service/ollama_ai_service.cc
  cli/service/proposal_registry.cc
  cli/service/resource_catalog.cc
  cli/service/rom_sandbox_manager.cc
  cli/service/policy_evaluator.cc
  cli/service/test_suite.h
  cli/service/test_suite_loader.cc
  cli/service/test_suite_loader.h
  cli/service/test_suite_reporter.cc
  cli/service/test_suite_reporter.h
  cli/service/test_suite_writer.cc
  cli/service/test_suite_writer.h
  cli/service/gemini_ai_service.cc
  app/rom.cc
  app/core/project.cc
  app/core/asar_wrapper.cc
  ${FILE_DIALOG_SRC}
  ${YAZE_APP_EMU_SRC}
  ${YAZE_APP_GFX_SRC}
  ${YAZE_APP_ZELDA3_SRC}
  ${YAZE_UTIL_SRC}
  ${YAZE_GUI_SRC}
  ${IMGUI_SRC}
)

option(YAZE_WITH_JSON "Build with JSON support" OFF)
if(YAZE_WITH_JSON)
  add_subdirectory(../../third_party/json)
  target_compile_definitions(z3ed PRIVATE YAZE_WITH_JSON)
  target_link_libraries(z3ed PRIVATE nlohmann_json::nlohmann_json)
  list(APPEND Z3ED_SRC_FILES cli/gemini_ai_service.cc)
endif()

target_include_directories(
  z3ed PUBLIC
  ${CMAKE_SOURCE_DIR}/src/lib/
  ${CMAKE_SOURCE_DIR}/src/app/
  ${CMAKE_SOURCE_DIR}/src/lib/asar/src
  ${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar
  ${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar-dll-bindings/c
  ${CMAKE_SOURCE_DIR}/incl/
  ${CMAKE_SOURCE_DIR}/src/
  ${CMAKE_SOURCE_DIR}/src/lib/imgui_test_engine
  ${PNG_INCLUDE_DIRS}
  ${SDL2_INCLUDE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
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
  ${CMAKE_DL_LIBS}
  ImGuiTestEngine
  ImGui
)

# ============================================================================
# Optional gRPC Support for CLI Agent Test Command
# ============================================================================
if(YAZE_WITH_GRPC)
  message(STATUS "Adding gRPC support to z3ed CLI")
  
  # Generate C++ code from .proto using the helper function from cmake/grpc.cmake
  target_add_protobuf(z3ed 
    ${CMAKE_SOURCE_DIR}/src/app/core/proto/imgui_test_harness.proto)
  
  # Add CLI gRPC service sources
  target_sources(z3ed PRIVATE
    ${CMAKE_SOURCE_DIR}/src/cli/service/gui_automation_client.cc
    ${CMAKE_SOURCE_DIR}/src/cli/service/gui_automation_client.h
    ${CMAKE_SOURCE_DIR}/src/cli/service/test_workflow_generator.cc
    ${CMAKE_SOURCE_DIR}/src/cli/service/test_workflow_generator.h)
  
  # Link gRPC libraries
  target_link_libraries(z3ed PRIVATE
    grpc++
    grpc++_reflection
    libprotobuf)
  
  message(STATUS "✓ gRPC CLI automation integrated")
endif()
