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

find_package(yaml-cpp CONFIG)
if(NOT yaml-cpp_FOUND)
  message(STATUS "yaml-cpp not found via package config, fetching from source")
  FetchContent_Declare(yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG 0.8.0
  )
  FetchContent_GetProperties(yaml-cpp)
  if(NOT yaml-cpp_POPULATED)
    FetchContent_Populate(yaml-cpp)

    set(_yaml_cpp_cmakelists "${yaml-cpp_SOURCE_DIR}/CMakeLists.txt")
    if(EXISTS "${_yaml_cpp_cmakelists}")
      file(READ "${_yaml_cpp_cmakelists}" _yaml_cpp_cmake_contents)
      if(_yaml_cpp_cmake_contents MATCHES "cmake_minimum_required\\(VERSION 3\\.4\\)")
        string(REPLACE "cmake_minimum_required(VERSION 3.4)"
                       "cmake_minimum_required(VERSION 3.5)"
                       _yaml_cpp_cmake_contents "${_yaml_cpp_cmake_contents}")
        file(WRITE "${_yaml_cpp_cmakelists}" "${_yaml_cpp_cmake_contents}")
      endif()
    endif()

    set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "Disable yaml-cpp tests" FORCE)
    set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "Disable yaml-cpp contrib" FORCE)
    set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "Disable yaml-cpp tools" FORCE)
    set(YAML_CPP_INSTALL OFF CACHE BOOL "Disable yaml-cpp install" FORCE)
    set(YAML_CPP_FORMAT_SOURCE OFF CACHE BOOL "Disable yaml-cpp format target" FORCE)

    add_subdirectory(${yaml-cpp_SOURCE_DIR} ${yaml-cpp_BINARY_DIR} EXCLUDE_FROM_ALL)
  endif()
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
  cli/handlers/overworld_inspect.cc
  cli/handlers/sprite.cc
  cli/handlers/project.cc
  cli/handlers/command_palette.cc
  cli/handlers/message.cc
  cli/handlers/agent.cc
  cli/handlers/agent/common.cc
  cli/handlers/agent/general_commands.cc
  cli/handlers/agent/conversation_test.cc
  cli/handlers/agent/test_common.cc
  cli/handlers/agent/test_commands.cc
  cli/handlers/agent/gui_commands.cc
  cli/handlers/agent/tool_commands.cc
    cli/flags.cc
  cli/modern_cli.cc
  cli/tui/asar_patch.cc
  cli/tui/palette_editor.cc
  cli/tui/command_palette.cc
  cli/tui/chat_tui.cc
  cli/service/testing/test_suite_loader.cc
  cli/service/testing/test_suite_reporter.cc
  cli/service/testing/test_suite_writer.cc
)

# ============================================================================
# AI Agent Support (Consolidated via Z3ED_AI flag)
# ============================================================================
if(Z3ED_AI OR YAZE_WITH_JSON)
  target_compile_definitions(z3ed PRIVATE YAZE_WITH_JSON)
  message(STATUS "✓ z3ed AI agent enabled (Ollama + Gemini support)")
  
  # Link nlohmann_json (already fetched in main CMakeLists if YAZE_WITH_JSON)
  target_link_libraries(z3ed PRIVATE nlohmann_json::nlohmann_json)
endif()

# ============================================================================
# SSL/HTTPS Support (Optional - Required for Gemini API)
# ============================================================================
# SSL is only enabled when AI features are active
# Ollama (localhost) works without SSL, Gemini (HTTPS) requires it
if((Z3ED_AI OR YAZE_WITH_JSON) AND (YAZE_WITH_GRPC OR Z3ED_AI))
  find_package(OpenSSL)
  
  if(OpenSSL_FOUND)
    # Define the SSL support macro for httplib
    target_compile_definitions(z3ed PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
    
    # Link OpenSSL libraries
    target_link_libraries(z3ed PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    
    # On macOS, also enable Keychain cert support
    if(APPLE)
      target_compile_definitions(z3ed PRIVATE CPPHTTPLIB_USE_CERTS_FROM_MACOSX_KEYCHAIN)
      target_link_libraries(z3ed PRIVATE "-framework CoreFoundation" "-framework Security")
    endif()
    
    message(STATUS "✓ SSL/HTTPS support enabled for z3ed (Gemini API ready)")
  else()
    message(WARNING "OpenSSL not found - Gemini API will not work")
    message(STATUS "  • Ollama (local) still works without SSL")
    message(STATUS "  • Install OpenSSL for Gemini: brew install openssl (macOS) or apt install libssl-dev (Linux)")
  endif()
else()
  if(NOT Z3ED_AI AND NOT YAZE_WITH_JSON)
    message(STATUS "○ z3ed AI agent disabled (set -DZ3ED_AI=ON to enable Gemini/Ollama)")
  endif()
endif()

target_include_directories(
  z3ed PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/incl
  ${CMAKE_SOURCE_DIR}/third_party/httplib
  ${SDL2_INCLUDE_DIR}
  ${PROJECT_BINARY_DIR}
)

if(YAZE_USE_MODULAR_BUILD)
  target_link_libraries(
    z3ed PRIVATE
    yaze_core
    ftxui::component
  )
else()
  target_link_libraries(
    z3ed PRIVATE
    yaze_core
    ftxui::component
    absl::flags
    absl::flags_parse
  )
endif()

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
    ${CMAKE_SOURCE_DIR}/src/cli/service/gui/gui_automation_client.cc
    ${CMAKE_SOURCE_DIR}/src/cli/service/gui/gui_automation_client.h
    ${CMAKE_SOURCE_DIR}/src/cli/service/testing/test_workflow_generator.cc
    ${CMAKE_SOURCE_DIR}/src/cli/service/testing/test_workflow_generator.h)
  
  # Link gRPC libraries
  target_link_libraries(z3ed PRIVATE
    grpc++
    grpc++_reflection
    libprotobuf)
  
  message(STATUS "✓ gRPC CLI automation integrated")
endif()
