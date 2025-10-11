set(YAZE_AGENT_SOURCES
  cli/service/agent/proposal_executor.cc
  cli/handlers/agent/todo_commands.cc
  cli/service/agent/conversational_agent_service.cc
  cli/service/agent/simple_chat_session.cc
  cli/service/agent/enhanced_tui.cc
  cli/service/agent/tool_dispatcher.cc
  cli/service/agent/learned_knowledge_service.cc
  cli/service/agent/todo_manager.cc
  cli/service/agent/vim_mode.cc
  cli/service/ai/ai_service.cc
  cli/service/ai/ai_action_parser.cc
  cli/service/ai/vision_action_refiner.cc
  cli/service/ai/ai_gui_controller.cc
  cli/service/ai/ollama_ai_service.cc
  cli/service/ai/prompt_builder.cc
  cli/service/ai/service_factory.cc
  cli/service/gui/gui_action_generator.cc
  cli/service/gui/gui_automation_client.cc
  cli/service/net/z3ed_network_client.cc
  cli/handlers/net/net_commands.cc
  cli/service/planning/policy_evaluator.cc
  cli/service/planning/proposal_registry.cc
  cli/service/planning/tile16_proposal_generator.cc
  cli/service/resources/resource_catalog.cc
  cli/service/resources/resource_context_builder.cc
  cli/service/resources/command_context.cc
  cli/service/resources/command_handler.cc
  cli/handlers/agent.cc
  cli/handlers/command_handlers.cc
  cli/handlers/agent/simple_chat_command.cc
  cli/handlers/game/overworld_inspect.cc
  cli/handlers/game/message.cc
  cli/handlers/rom/mock_rom.cc
  # CommandHandler-based implementations
  cli/handlers/tools/resource_commands.cc
  cli/handlers/game/dungeon_commands.cc
  cli/handlers/game/overworld_commands.cc
  cli/handlers/tools/gui_commands.cc
  cli/handlers/graphics/hex_commands.cc
  cli/handlers/game/dialogue_commands.cc
  cli/handlers/game/music_commands.cc
  cli/handlers/graphics/palette_commands.cc
  cli/handlers/tools/emulator_commands.cc
  cli/handlers/game/message_commands.cc
  cli/handlers/graphics/sprite_commands.cc
  # ROM commands
  cli/handlers/rom/rom_commands.cc
  cli/handlers/rom/project_commands.cc
  cli/flags.cc
  cli/service/rom/rom_sandbox_manager.cc
)

# gRPC-dependent sources (only added when gRPC is enabled)
if(YAZE_WITH_GRPC)
  list(APPEND YAZE_AGENT_SOURCES
    cli/service/agent/agent_control_server.cc
    cli/service/agent/emulator_service_impl.cc
  )
endif()

if(YAZE_WITH_JSON)
  list(APPEND YAZE_AGENT_SOURCES cli/service/ai/gemini_ai_service.cc)
endif()

add_library(yaze_agent STATIC ${YAZE_AGENT_SOURCES})

set(_yaze_agent_link_targets
  yaze_common
  yaze_util
  yaze_gfx
  yaze_gui
  yaze_core_lib
  yaze_zelda3
  yaze_emulator
  ${ABSL_TARGETS}
  yaml-cpp
)

target_link_libraries(yaze_agent PUBLIC ${_yaze_agent_link_targets})

target_include_directories(yaze_agent
  PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/incl
    ${CMAKE_SOURCE_DIR}/third_party/httplib
    ${CMAKE_SOURCE_DIR}/third_party/json/include
    ${CMAKE_SOURCE_DIR}/src/lib
    ${CMAKE_SOURCE_DIR}/src/cli/handlers
)

if(SDL2_INCLUDE_DIR)
  target_include_directories(yaze_agent PUBLIC ${SDL2_INCLUDE_DIR})
endif()

if(YAZE_WITH_JSON)
  target_link_libraries(yaze_agent PUBLIC nlohmann_json::nlohmann_json)
  target_compile_definitions(yaze_agent PUBLIC YAZE_WITH_JSON)

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
endif()

# Add gRPC support for GUI automation
if(YAZE_WITH_GRPC)
  # Generate proto files for yaze_agent
  target_add_protobuf(yaze_agent
    ${PROJECT_SOURCE_DIR}/src/protos/imgui_test_harness.proto
    ${PROJECT_SOURCE_DIR}/src/protos/canvas_automation.proto
    ${PROJECT_SOURCE_DIR}/src/protos/emulator_service.proto)
  
  target_link_libraries(yaze_agent PUBLIC
    grpc++
    grpc++_reflection
    libprotobuf
  )
  
  # Note: YAZE_WITH_GRPC is defined globally via add_compile_definitions in root CMakeLists.txt
  # This ensures #ifdef YAZE_WITH_GRPC works in all translation units
  message(STATUS "✓ gRPC GUI automation enabled for yaze_agent")
endif()

set_target_properties(yaze_agent PROPERTIES POSITION_INDEPENDENT_CODE ON)
