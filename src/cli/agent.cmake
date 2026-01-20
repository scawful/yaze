set(_YAZE_NEEDS_AGENT FALSE)

# Agent library is not compatible with Emscripten/WASM due to dependencies on:
# - OpenSSL for HTTPS support
# - Threading libraries that aren't fully compatible with WASM
# - Network libraries that require native sockets
# However, we can provide browser-based AI services using the Fetch API
if(EMSCRIPTEN)
  # Create a minimal browser-based AI service library for WASM
  set(YAZE_BROWSER_AI_SOURCES
    cli/service/ai/browser_ai_service.cc
    cli/service/ai/ai_service.cc
    cli/service/ai/common.h
    cli/wasm_terminal_bridge.cc  # Web terminal integration

    # Browser specific implementations
    cli/service/ai/service_factory_browser.cc
    cli/service/agent/todo_manager.cc
    
    # Proposal and Sandbox support (needed by yaze_editor)
    cli/service/planning/proposal_registry.cc
    cli/service/planning/tile16_proposal_generator.cc
    cli/service/rom/rom_sandbox_manager.cc
    # Core Agent Service (Critical for WASM Agent API)
    cli/service/agent/conversational_agent_service.cc
    cli/service/agent/tool_dispatcher.cc
    cli/service/agent/tool_registry.cc
    cli/service/agent/learned_knowledge_service.cc
    cli/service/agent/agent_pretraining.cc
    cli/service/agent/proposal_executor.cc

    # Additional tools required by ToolDispatcher
    cli/service/agent/tools/filesystem_tool.cc
    cli/service/agent/tools/memory_inspector_tool.cc
    cli/service/agent/tools/visual_analysis_tool.cc
    cli/service/agent/tools/code_gen_tool.cc
    cli/service/agent/tools/project_tool.cc
    cli/service/agent/tools/build_tool.cc
    cli/service/agent/tools/rom_diff_tool.cc
    cli/service/agent/tools/validation_tool.cc
  )

  add_library(yaze_agent STATIC ${YAZE_BROWSER_AI_SOURCES})

  target_link_libraries(yaze_agent PUBLIC
    yaze_cli_core
    yaze_common
    yaze_util
    yaze_app_core_lib  # For Rom class and core functionality
    yaze_zelda3        # For game-specific structures
    ${ABSL_TARGETS}
  )

  # Link with the network abstraction layer for HTTP client
  if(TARGET yaze_net)
    target_link_libraries(yaze_agent PUBLIC yaze_net)
  endif()

  # Add JSON support for API communication
  if(YAZE_ENABLE_JSON)
    target_link_libraries(yaze_agent PUBLIC nlohmann_json::nlohmann_json)
    target_compile_definitions(yaze_agent PUBLIC YAZE_WITH_JSON)
  endif()

  target_include_directories(yaze_agent PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/incl
  )

  set_target_properties(yaze_agent PROPERTIES POSITION_INDEPENDENT_CODE ON)

  message(STATUS "yaze_agent configured for WASM with browser-based AI services")
  return()
endif()

if(YAZE_ENABLE_AGENT_CLI AND (YAZE_BUILD_CLI OR YAZE_BUILD_Z3ED))
  set(_YAZE_NEEDS_AGENT TRUE)
endif()
if(YAZE_BUILD_AGENT_UI)
  set(_YAZE_NEEDS_AGENT TRUE)
endif()
if(YAZE_BUILD_TESTS AND NOT YAZE_MINIMAL_BUILD)
  set(_YAZE_NEEDS_AGENT TRUE)
endif()

if(NOT _YAZE_NEEDS_AGENT)
  add_library(yaze_agent INTERFACE)
  message(STATUS "yaze_agent stubbed out (agent CLI/UI disabled)")
  return()
endif()

set(YAZE_AGENT_CORE_SOURCES
  # Agent runtime and services
  cli/service/agent/conversational_agent_service.cc
  cli/service/agent/dev_assist_agent.cc
  cli/service/agent/enhanced_tui.cc
  cli/service/agent/learned_knowledge_service.cc
  cli/service/agent/prompt_manager.cc
  cli/service/agent/simple_chat_session.cc
  cli/service/agent/todo_manager.cc
  cli/service/agent/tool_dispatcher.cc
  cli/service/agent/tool_registration.cc
  cli/service/agent/tool_registry.cc
  cli/service/agent/tools/build_tool.cc
  cli/service/agent/tools/code_gen_tool.cc
  cli/service/agent/tools/filesystem_tool.cc
  cli/service/agent/tools/memory_inspector_tool.cc
  cli/service/agent/tools/project_tool.cc
  cli/service/agent/tools/rom_diff_tool.cc
  cli/service/agent/tools/validation_tool.cc
  cli/service/agent/tools/visual_analysis_tool.cc
  cli/service/agent/disassembler_65816.cc
  cli/service/agent/vim_mode.cc

  cli/service/gui/gui_action_generator.cc
  cli/service/planning/policy_evaluator.cc
  cli/service/planning/proposal_registry.cc
  cli/service/planning/tile16_proposal_generator.cc
  cli/service/rom/rom_sandbox_manager.cc
  cli/service/agent/proposal_executor.cc

  cli/service/ai/ai_action_parser.cc
  cli/service/ai/ai_service.cc
  cli/service/ai/model_registry.cc
  cli/service/ai/vision_action_refiner.cc
  cli/service/api/http_server.cc
  cli/service/api/api_handlers.cc
  
  app/editor/agent/agent_chat.cc # New unified chat component
  app/editor/agent/agent_editor.cc
  app/editor/agent/panels/agent_editor_panels.cc
)

if(YAZE_ENABLE_REMOTE_AUTOMATION)
  list(APPEND YAZE_AGENT_CORE_SOURCES
    cli/service/agent/rom_debug_agent.cc
  )
endif()

# AI runtime sources
if(YAZE_ENABLE_AI_RUNTIME)
  list(APPEND YAZE_AGENT_CORE_SOURCES
    cli/service/agent/advanced_routing.cc
    cli/service/agent/agent_pretraining.cc
    cli/service/ai/ai_gui_controller.cc
    cli/service/ai/ollama_ai_service.cc
    cli/service/ai/local_gemini_cli_service.cc
    cli/service/ai/prompt_builder.cc
    cli/service/ai/service_factory.cc
  )
else()
  list(APPEND YAZE_AGENT_CORE_SOURCES
    cli/service/ai/service_factory_stub.cc
  )
endif()

set(YAZE_AGENT_SOURCES ${YAZE_AGENT_CORE_SOURCES})

# gRPC-dependent sources (only added when remote automation is enabled)
if(YAZE_ENABLE_REMOTE_AUTOMATION)
  list(APPEND YAZE_AGENT_SOURCES
    cli/service/agent/agent_control_server.cc
    cli/service/agent/emulator_service_impl.cc
  )
endif()

if(YAZE_ENABLE_AI_RUNTIME AND YAZE_ENABLE_JSON)
  list(APPEND YAZE_AGENT_SOURCES
    cli/service/ai/gemini_ai_service.cc
    cli/service/ai/openai_ai_service.cc
    cli/service/ai/anthropic_ai_service.cc
  )
endif()

add_library(yaze_agent STATIC ${YAZE_AGENT_SOURCES})

set(_yaze_agent_link_targets
  yaze_cli_core
  yaze_common
  yaze_util
  yaze_gfx
  yaze_gui
  yaze_app_core_lib
  yaze_zelda3
  yaze_emulator
  ${ABSL_TARGETS}
)

# Only include ftxui targets if CLI is being built
# ftxui is not available in WASM/Emscripten builds
if(YAZE_BUILD_CLI AND NOT EMSCRIPTEN)
  list(APPEND _yaze_agent_link_targets
    ftxui::screen
    ftxui::dom
    ftxui::component
  )
endif()

if(YAZE_ENABLE_AI_RUNTIME)
  # Prefer the consolidated yaml target so include paths propagate consistently
  if(DEFINED YAZE_YAML_TARGETS AND NOT "${YAZE_YAML_TARGETS}" STREQUAL "")
    list(APPEND _yaze_agent_link_targets ${YAZE_YAML_TARGETS})
  else()
    # Fallback in case dependency setup changes
    list(APPEND _yaze_agent_link_targets yaml-cpp)
  endif()
endif()

target_link_libraries(yaze_agent PUBLIC ${_yaze_agent_link_targets})

if(NOT EMSCRIPTEN AND YAZE_HTTPLIB_TARGETS)
  target_link_libraries(yaze_agent PUBLIC ${YAZE_HTTPLIB_TARGETS})
endif()

# Ensure yaml-cpp include paths propagate even when using system packages
if(YAZE_ENABLE_AI_RUNTIME)
  set(_yaml_targets_to_check ${YAZE_YAML_TARGETS} yaml-cpp yaml-cpp::yaml-cpp)
  foreach(_yaml_target IN LISTS _yaml_targets_to_check)
    if(TARGET ${_yaml_target})
      get_target_property(_yaml_inc ${_yaml_target} INTERFACE_INCLUDE_DIRECTORIES)
      if(_yaml_inc)
        target_include_directories(yaze_agent PUBLIC ${_yaml_inc})
        break()
      endif()
    endif()
  endforeach()
endif()

target_include_directories(yaze_agent
  PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/incl
    ${CMAKE_SOURCE_DIR}/src/lib
    ${CMAKE_SOURCE_DIR}/src/cli/handlers
    ${CMAKE_BINARY_DIR}/gens
)

if(SDL2_INCLUDE_DIR)
  target_include_directories(yaze_agent PUBLIC ${SDL2_INCLUDE_DIR})
endif()

if(YAZE_ENABLE_JSON)
  target_link_libraries(yaze_agent PUBLIC nlohmann_json::nlohmann_json)
  target_compile_definitions(yaze_agent PUBLIC YAZE_WITH_JSON)
endif()

if(YAZE_ENABLE_AI_RUNTIME AND YAZE_ENABLE_JSON)
  # Only link OpenSSL if gRPC is NOT enabled (to avoid duplicate symbol errors)
  # When gRPC is enabled, it brings its own OpenSSL which we'll use instead
  if(NOT YAZE_ENABLE_REMOTE_AUTOMATION)
    # CRITICAL FIX: Disable OpenSSL on Windows to avoid missing header errors
    # Windows CI doesn't have OpenSSL headers properly configured
    # HTTP API works fine without HTTPS for local development
    if(NOT WIN32)
      find_package(OpenSSL)
      if(OPENSSL_INCLUDE_DIR)
        target_include_directories(yaze_agent PUBLIC ${OPENSSL_INCLUDE_DIR})
      elseif(OPENSSL_ROOT_DIR)
        target_include_directories(yaze_agent PUBLIC ${OPENSSL_ROOT_DIR}/include)
      endif()
      if(OPENSSL_FOUND)
        if(TARGET OpenSSL::SSL)
          target_link_libraries(yaze_agent PUBLIC OpenSSL::SSL OpenSSL::Crypto)
        else()
          target_link_libraries(yaze_agent PUBLIC ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY})
        endif()
        target_compile_definitions(yaze_agent PUBLIC CPPHTTPLIB_OPENSSL_SUPPORT)

        if(APPLE)
          target_compile_definitions(yaze_agent PUBLIC CPPHTTPLIB_USE_CERTS_FROM_MACOSX_KEYCHAIN)
          target_link_libraries(yaze_agent PUBLIC "-framework CoreFoundation" "-framework Security")
        endif()

        message(STATUS "✓ SSL/HTTPS support enabled for yaze_agent (Gemini + HTTPS)")
      else()
        message(WARNING "OpenSSL not found - Gemini HTTPS features disabled (Ollama still works)")
        message(STATUS "  Install OpenSSL to enable Gemini: brew install openssl (macOS) or apt-get install libssl-dev (Linux)")
      endif()
    else()
      message(STATUS "Windows: HTTP API using plain HTTP (no SSL) - OpenSSL headers not available in CI")
    endif()
  else()
    # When gRPC is enabled, still enable OpenSSL features but use gRPC's OpenSSL
    target_compile_definitions(yaze_agent PUBLIC CPPHTTPLIB_OPENSSL_SUPPORT)
    if(APPLE)
      target_compile_definitions(yaze_agent PUBLIC CPPHTTPLIB_USE_CERTS_FROM_MACOSX_KEYCHAIN)
      target_link_libraries(yaze_agent PUBLIC "-framework CoreFoundation" "-framework Security")
    endif()
    message(STATUS "✓ SSL/HTTPS support enabled via gRPC's OpenSSL (Gemini + HTTPS)")
  endif()
endif()

# Add gRPC support for GUI automation
if(YAZE_ENABLE_REMOTE_AUTOMATION)
  # Link to consolidated gRPC support library
  target_link_libraries(yaze_agent PUBLIC yaze_grpc_support)
  
  # Ensure proto files are generated before yaze_agent compiles
  # yaze_proto_gen is an OBJECT library that generates the proto headers
  # This breaks the dependency cycle by separating proto generation from yaze_grpc_support
  if(TARGET yaze_proto_gen)
    add_dependencies(yaze_agent yaze_proto_gen)
    target_include_directories(yaze_agent PUBLIC ${CMAKE_BINARY_DIR}/gens)
  endif()
  
  # Note: YAZE_WITH_GRPC is defined globally via add_compile_definitions in options.cmake
  # This ensures #ifdef YAZE_WITH_GRPC works in all translation units
  message(STATUS "✓ gRPC GUI automation enabled for yaze_agent")
endif()

# Add OpenCV support for advanced visual analysis
if(YAZE_ENABLE_OPENCV AND OpenCV_FOUND)
  target_link_libraries(yaze_agent PUBLIC ${OpenCV_LIBS})
  target_include_directories(yaze_agent PUBLIC ${OpenCV_INCLUDE_DIRS})
  message(STATUS "✓ OpenCV visual analysis enabled for yaze_agent")
endif()

# NOTE: yaze_agent should NOT link to yaze_test_support to avoid circular dependency.
# The circular force-load chain (yaze_test_support -> yaze_agent -> yaze_test_support)
# causes SIGSEGV during static initialization due to duplicate symbols and SIOF.
#
# Test executables that need both should link them directly:
#   target_link_libraries(my_test PRIVATE yaze_test_support)
#
# yaze_test_support already force-loads yaze_agent, so agent symbols are available.

set_target_properties(yaze_agent PROPERTIES POSITION_INDEPENDENT_CODE ON)
