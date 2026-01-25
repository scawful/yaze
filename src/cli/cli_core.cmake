# Core CLI library (command registry + handlers)

set(_YAZE_CLI_COMMAND_HANDLERS
  cli/handlers/command_handlers.cc
)
if(EMSCRIPTEN)
  set(_YAZE_CLI_COMMAND_HANDLERS
    cli/handlers/command_handlers_browser.cc
  )
endif()

set(YAZE_CLI_CORE_SOURCES
  cli/flags.cc
  cli/service/command_registry.cc
  cli/service/resources/command_context.cc
  cli/service/resources/command_handler.cc
  cli/service/resources/resource_catalog.cc
  cli/service/resources/resource_context_builder.cc
  cli/handlers/mesen_handlers.cc
  ${_YAZE_CLI_COMMAND_HANDLERS}

  cli/handlers/graphics/gfx.cc
  cli/handlers/graphics/hex_commands.cc
  cli/handlers/graphics/palette.cc
  cli/handlers/graphics/palette_commands.cc
  cli/handlers/graphics/sprite_commands.cc

  cli/handlers/game/dialogue_commands.cc
  cli/handlers/game/dungeon_commands.cc
  cli/handlers/game/message.cc
  cli/handlers/game/message_commands.cc
  cli/handlers/game/music_commands.cc
  cli/handlers/game/overworld.cc
  cli/handlers/game/overworld_commands.cc
  cli/handlers/game/overworld_inspect.cc

  cli/handlers/net/net_commands.cc

  cli/handlers/rom/mock_rom.cc
  cli/handlers/rom/project_commands.cc
  cli/handlers/rom/rom_commands.cc

  cli/handlers/tools/dungeon_doctor_commands.cc
  cli/handlers/tools/graphics_doctor_commands.cc
  cli/handlers/tools/gui_commands.cc
  cli/handlers/tools/hex_inspector_commands.cc
  cli/handlers/tools/message_doctor_commands.cc
  cli/handlers/tools/overworld_doctor_commands.cc
  cli/handlers/tools/overworld_validate_commands.cc
  cli/handlers/tools/resource_commands.cc
  cli/handlers/tools/rom_compare_commands.cc
  cli/handlers/tools/rom_doctor_commands.cc
  cli/handlers/tools/sprite_doctor_commands.cc
  cli/handlers/tools/test_cli_commands.cc
  cli/handlers/tools/test_helpers_commands.cc

  cli/service/gui/canvas_automation_client.cc
  cli/service/gui/gui_automation_client.cc
  cli/service/net/z3ed_network_client.cc
  cli/service/rom/rom_sandbox_manager.cc
  cli/service/testing/test_suite_loader.cc
  cli/service/testing/test_suite_reporter.cc
  cli/service/testing/test_suite_writer.cc
  cli/service/testing/test_workflow_generator.cc
)

if(YAZE_ENABLE_GRPC)
  list(APPEND YAZE_CLI_CORE_SOURCES
    cli/handlers/tools/emulator_commands.cc
  )
endif()

add_library(yaze_cli_core STATIC ${YAZE_CLI_CORE_SOURCES})

set_target_properties(yaze_cli_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(yaze_cli_core PUBLIC
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/inc
  ${CMAKE_SOURCE_DIR}/src/lib
  ${CMAKE_BINARY_DIR}/gens
)

target_link_libraries(yaze_cli_core PUBLIC
  yaze_common
  yaze_util
  yaze_core_lib
  yaze_rom
  yaze_gfx
  yaze_zelda3
  yaze_emulator
  ${ABSL_TARGETS}
)

if(YAZE_ENABLE_JSON)
  target_link_libraries(yaze_cli_core PUBLIC nlohmann_json::nlohmann_json)
endif()

if(NOT EMSCRIPTEN AND YAZE_HTTPLIB_TARGETS)
  target_link_libraries(yaze_cli_core PUBLIC ${YAZE_HTTPLIB_TARGETS})
endif()

# When gRPC is enabled, cli_core needs proto files to be generated first
# (gui_automation_client.h includes protos/imgui_test_harness.grpc.pb.h)
if(YAZE_ENABLE_GRPC AND TARGET yaze_proto_gen)
  add_dependencies(yaze_cli_core yaze_proto_gen)
  target_link_libraries(yaze_cli_core PUBLIC yaze_grpc_support)
endif()

message(STATUS "✓ yaze_cli_core configured")
