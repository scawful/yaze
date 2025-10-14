# This file defines the z3ed command-line tool.

add_executable(z3ed
  cli/cli_main.cc
  cli/cli.cc
  cli/tui/tui.cc
  cli/tui/unified_layout.cc
  cli/tui/chat_tui.cc
  cli/tui/autocomplete_ui.cc
  cli/util/autocomplete.cc
  # ... (source files)
)

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
    "${CMAKE_CURRENT_SOURCE_DIR}/tui"
)

target_link_libraries(z3ed PRIVATE
    yaze_core
    yaze_agent
    ftxui::component
)

if(Z3ED_AI)
    target_link_libraries(z3ed PRIVATE yaml-cpp)
endif()

if(YAZE_WITH_GRPC)
  message(STATUS "Adding gRPC support to z3ed CLI")
  target_link_libraries(z3ed PRIVATE grpc++ grpc++_reflection libprotobuf)
  
  # On Windows, force whole-archive linking for protobuf to ensure all symbols are included
if(MSVC AND CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  target_link_options(z3ed PRIVATE /WHOLEARCHIVE:$<TARGET_FILE:libprotobuf>)
elseif(MSVC)
  message(STATUS "○ Skipping /WHOLEARCHIVE for libprotobuf in z3ed (clang-cl)")
endif()
endif()
