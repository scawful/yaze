# This file defines the z3ed command-line tool.

# Define base source files
set(Z3ED_BASE_SOURCES
  cli/cli_main.cc
  cli/cli.cc
  cli/util/autocomplete.cc
)

# Add WASM terminal bridge for Emscripten builds
if(EMSCRIPTEN)
  list(APPEND Z3ED_BASE_SOURCES
    cli/wasm_terminal_bridge.cc
    cli/wasm_z3ed_stub.cc
  )
endif()

add_executable(z3ed ${Z3ED_BASE_SOURCES})

target_compile_definitions(z3ed PRIVATE YAZE_ASSETS_PATH="${CMAKE_SOURCE_DIR}/assets")

# Copy agent assets for z3ed
if(EXISTS ${CMAKE_SOURCE_DIR}/assets/agent)
  file(COPY ${CMAKE_SOURCE_DIR}/assets/agent/ DESTINATION "${CMAKE_BINARY_DIR}/assets/agent/")
  add_custom_command(TARGET z3ed POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets/agent $<TARGET_FILE_DIR:z3ed>/assets/agent
    COMMENT "Copying agent assets for z3ed"
  )
endif()

target_include_directories(z3ed PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(z3ed PRIVATE
    yaze_cli_core
)

if(YAZE_ENABLE_AGENT_CLI)
  target_link_libraries(z3ed PRIVATE yaze_cli_agent)
  target_compile_definitions(z3ed PRIVATE YAZE_ENABLE_AGENT_CLI)
endif()

if(Z3ED_AI)
    target_link_libraries(z3ed PRIVATE yaml-cpp)
endif()

if(YAZE_WITH_GRPC)
  message(STATUS "Adding gRPC support to z3ed CLI")
  # Link to consolidated gRPC support library
  target_link_libraries(z3ed PRIVATE yaze_grpc_support)
endif()

# Note: yaze_app_core_lib was previously linked here for FileDialogWrapper
# symbols, but z3ed CLI does not use file dialogs. Removed to avoid pulling
# in the entire editor GUI dependency graph.

# Add Emscripten-specific flags for WASM builds
if(EMSCRIPTEN)
  message(STATUS "Configuring z3ed for WASM terminal mode")
  # Export the actual C functions from wasm_terminal_bridge.cc
  set_target_properties(z3ed PROPERTIES
    LINK_FLAGS "-s EXPORTED_FUNCTIONS='[\"_main\",\"_Z3edProcessCommand\",\"_Z3edIsReady\",\"_Z3edGetCompletions\",\"_Z3edSetApiKey\",\"_Z3edLoadRomData\",\"_Z3edGetRomInfo\",\"_Z3edQueryResource\"]' -s EXPORTED_RUNTIME_METHODS='[\"ccall\",\"cwrap\",\"stringToUTF8\",\"UTF8ToString\"]' -s MODULARIZE=1 -s EXPORT_NAME='Z3edTerminal' --bind"
  )
  target_compile_definitions(z3ed PRIVATE YAZE_WASM_TERMINAL_MODE)
endif()
