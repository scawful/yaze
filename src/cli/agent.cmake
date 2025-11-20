set(_YAZE_NEEDS_AGENT FALSE)
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
  # Core infrastructure
  cli/flags.cc
  cli/handlers/agent.cc
  cli/handlers/agent/common.cc
  cli/handlers/agent/conversation_test.cc
  cli/handlers/agent/general_commands.cc
  cli/handlers/agent/simple_chat_command.cc
  cli/handlers/agent/test_commands.cc
  cli/handlers/agent/test_common.cc
  cli/handlers/agent/todo_commands.cc
  cli/handlers/command_handlers.cc
  cli/handlers/game/dialogue_commands.cc
  cli/handlers/game/dungeon.cc
  cli/handlers/game/dungeon_commands.cc
  cli/handlers/game/message.cc
  cli/handlers/game/message_commands.cc
  cli/handlers/game/music_commands.cc
  cli/handlers/game/overworld.cc
  cli/handlers/game/overworld_commands.cc
  cli/handlers/game/overworld_inspect.cc
  cli/handlers/graphics/gfx.cc
  cli/handlers/graphics/hex_commands.cc
  cli/handlers/graphics/palette.cc
  cli/handlers/graphics/palette_commands.cc
  cli/handlers/graphics/sprite_commands.cc
  cli/handlers/net/net_commands.cc
  cli/handlers/rom/mock_rom.cc
  cli/handlers/rom/project_commands.cc
  cli/handlers/rom/rom_commands.cc
  cli/handlers/tools/gui_commands.cc
  cli/handlers/tools/resource_commands.cc
  cli/service/agent/conversational_agent_service.cc
  cli/service/agent/enhanced_tui.cc
  cli/service/agent/learned_knowledge_service.cc
  cli/service/agent/prompt_manager.cc
  cli/service/agent/simple_chat_session.cc
  cli/service/agent/todo_manager.cc
  cli/service/agent/tool_dispatcher.cc
  cli/service/agent/vim_mode.cc
  cli/service/command_registry.cc
  cli/service/gui/gui_action_generator.cc
  cli/service/net/z3ed_network_client.cc
  cli/service/planning/policy_evaluator.cc
  cli/service/planning/proposal_registry.cc
  cli/service/resources/command_context.cc
  cli/service/resources/command_handler.cc
  cli/service/resources/resource_catalog.cc
  cli/service/resources/resource_context_builder.cc
  cli/service/rom/rom_sandbox_manager.cc
  cli/service/agent/proposal_executor.cc
  cli/service/testing/test_suite_loader.cc
  cli/service/testing/test_suite_reporter.cc
  cli/service/testing/test_suite_writer.cc
  cli/service/testing/test_workflow_generator.cc
  cli/service/ai/ai_service.cc
  cli/service/ai/model_registry.cc
  cli/service/api/http_server.cc
  cli/service/api/api_handlers.cc
  
  # Advanced features
  # CommandHandler-based implementations
  # ROM commands
)

# AI runtime sources
if(YAZE_ENABLE_AI_RUNTIME)
  list(APPEND YAZE_AGENT_CORE_SOURCES
    cli/service/agent/advanced_routing.cc
    cli/service/agent/agent_pretraining.cc
    cli/service/ai/ai_action_parser.cc
    cli/service/ai/ai_gui_controller.cc
    cli/service/ai/ollama_ai_service.cc
    cli/service/ai/prompt_builder.cc
    cli/service/ai/service_factory.cc
    cli/service/ai/vision_action_refiner.cc
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
    cli/handlers/tools/emulator_commands.cc
    cli/service/gui/gui_automation_client.cc
    cli/service/planning/tile16_proposal_generator.cc
  )
endif()

if(YAZE_ENABLE_AI_RUNTIME AND YAZE_ENABLE_JSON)
  list(APPEND YAZE_AGENT_SOURCES cli/service/ai/gemini_ai_service.cc)
endif()

add_library(yaze_agent STATIC ${YAZE_AGENT_SOURCES})

set(_yaze_agent_link_targets
  yaze_common
  yaze_util
  yaze_gfx
  yaze_gui
  yaze_app_core_lib
  yaze_zelda3
  yaze_emulator
  ${ABSL_TARGETS}
  ftxui::screen
  ftxui::dom
  ftxui::component
)

if(YAZE_ENABLE_AI_RUNTIME)
  list(APPEND _yaze_agent_link_targets yaml-cpp)
endif()

target_link_libraries(yaze_agent PUBLIC ${_yaze_agent_link_targets})

target_include_directories(yaze_agent
  PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/incl
    ${CMAKE_SOURCE_DIR}/ext/httplib
    ${CMAKE_SOURCE_DIR}/src/lib
    ${CMAKE_SOURCE_DIR}/src/cli/handlers
)

if(YAZE_ENABLE_AI_RUNTIME AND YAZE_ENABLE_JSON)
  target_include_directories(yaze_agent PUBLIC ${CMAKE_SOURCE_DIR}/ext/json/include)
endif()

if(SDL2_INCLUDE_DIR)
  target_include_directories(yaze_agent PUBLIC ${SDL2_INCLUDE_DIR})
endif()

if(YAZE_ENABLE_AI_RUNTIME AND YAZE_ENABLE_JSON)
  target_link_libraries(yaze_agent PUBLIC nlohmann_json::nlohmann_json)
  target_compile_definitions(yaze_agent PUBLIC YAZE_WITH_JSON)

  # Only link OpenSSL if gRPC is NOT enabled (to avoid duplicate symbol errors)
  # When gRPC is enabled, it brings its own OpenSSL which we'll use instead
  if(NOT YAZE_ENABLE_REMOTE_AUTOMATION)
    find_package(OpenSSL)
    if(OpenSSL_FOUND)
      target_compile_definitions(yaze_agent PUBLIC CPPHTTPLIB_OPENSSL_SUPPORT)
      target_link_libraries(yaze_agent PUBLIC OpenSSL::SSL OpenSSL::Crypto)

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
  
  # Note: YAZE_WITH_GRPC is defined globally via add_compile_definitions in options.cmake
  # This ensures #ifdef YAZE_WITH_GRPC works in all translation units
  message(STATUS "✓ gRPC GUI automation enabled for yaze_agent")
endif()

# Link test support when tests are enabled (agent uses test harness functions)
if(YAZE_BUILD_TESTS AND TARGET yaze_test_support)
  if(APPLE)
    target_link_options(yaze_agent PUBLIC 
      "LINKER:-force_load,$<TARGET_FILE:yaze_test_support>")
    target_link_libraries(yaze_agent PUBLIC yaze_test_support)
  elseif(UNIX)
    target_link_libraries(yaze_agent PUBLIC 
      -Wl,--whole-archive yaze_test_support -Wl,--no-whole-archive)
  else()
    # Windows: Normal linking
    target_link_libraries(yaze_agent PUBLIC yaze_test_support)
  endif()
  message(STATUS "✓ yaze_agent linked to yaze_test_support")
endif()

set_target_properties(yaze_agent PROPERTIES POSITION_INDEPENDENT_CODE ON)
