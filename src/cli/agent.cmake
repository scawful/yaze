set(YAZE_AGENT_SOURCES
  cli/handlers/agent/tool_commands.cc
  cli/service/agent/conversational_agent_service.cc
  cli/service/agent/tool_dispatcher.cc
  cli/service/ai/ai_service.cc
  cli/service/ai/ollama_ai_service.cc
  cli/service/ai/prompt_builder.cc
  cli/service/ai/service_factory.cc
  cli/service/planning/policy_evaluator.cc
  cli/service/planning/proposal_registry.cc
  cli/service/planning/tile16_proposal_generator.cc
  cli/service/resources/resource_catalog.cc
  cli/service/resources/resource_context_builder.cc
  cli/service/rom/rom_sandbox_manager.cc
)

if(YAZE_WITH_JSON)
  list(APPEND YAZE_AGENT_SOURCES cli/service/ai/gemini_ai_service.cc)
endif()

add_library(yaze_agent STATIC ${YAZE_AGENT_SOURCES})

target_link_libraries(yaze_agent
  PUBLIC
    yaze_common
    ${ABSL_TARGETS}
    yaml-cpp
)

target_include_directories(yaze_agent
  PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/incl
    ${CMAKE_SOURCE_DIR}/third_party/httplib
    ${CMAKE_SOURCE_DIR}/third_party/json/include
    ${CMAKE_SOURCE_DIR}/src/lib
)

if(SDL2_INCLUDE_DIR)
  target_include_directories(yaze_agent PUBLIC ${SDL2_INCLUDE_DIR})
endif()

if(YAZE_WITH_JSON)
  target_link_libraries(yaze_agent PUBLIC nlohmann_json::nlohmann_json)
  target_compile_definitions(yaze_agent PUBLIC YAZE_WITH_JSON)
endif()

set_target_properties(yaze_agent PROPERTIES POSITION_INDEPENDENT_CODE ON)
