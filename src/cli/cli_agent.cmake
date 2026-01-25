# CLI agent commands (agent subcommands + simple-chat)

set(YAZE_CLI_AGENT_SOURCES
  cli/handlers/agent.cc
  cli/handlers/agent/common.cc
  cli/handlers/agent/conversation_test.cc
  cli/handlers/agent/general_commands.cc
  cli/handlers/agent/simple_chat_command.cc
  cli/handlers/agent/test_commands.cc
  cli/handlers/agent/test_common.cc
  cli/handlers/agent/todo_commands.cc
  cli/handlers/command_handlers_agent.cc
  cli/handlers/agent_command_registration.cc
)

if(EMSCRIPTEN)
  set(YAZE_CLI_AGENT_SOURCES
    cli/handlers/agent/browser_agent.cc
    cli/handlers/agent/todo_commands.cc
    cli/handlers/command_handlers_agent.cc
    cli/handlers/agent_command_registration.cc
  )
endif()

add_library(yaze_cli_agent STATIC ${YAZE_CLI_AGENT_SOURCES})

set_target_properties(yaze_cli_agent PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(yaze_cli_agent PUBLIC
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/inc
  ${CMAKE_SOURCE_DIR}/src/lib
)

target_link_libraries(yaze_cli_agent PUBLIC
  yaze_agent
  yaze_cli_core
  ${ABSL_TARGETS}
)

message(STATUS "✓ yaze_cli_agent configured")
