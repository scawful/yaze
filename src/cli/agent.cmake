include(FetchContent)

function(_yaze_ensure_yaml_cpp _out_target)
  if(TARGET yaml-cpp::yaml-cpp)
    set(${_out_target} yaml-cpp::yaml-cpp PARENT_SCOPE)
    return()
  endif()

  if(TARGET yaml-cpp)
    set(${_out_target} yaml-cpp PARENT_SCOPE)
    return()
  endif()

  find_package(yaml-cpp CONFIG QUIET)

  if(TARGET yaml-cpp::yaml-cpp)
    set(${_out_target} yaml-cpp::yaml-cpp PARENT_SCOPE)
    return()
  elseif(TARGET yaml-cpp)
    set(${_out_target} yaml-cpp PARENT_SCOPE)
    return()
  endif()

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

    if(NOT TARGET yaml-cpp)
      message(FATAL_ERROR "yaml-cpp target was not created after fetching")
    endif()

    # Ensure the fetched target exposes its public headers
    target_include_directories(yaml-cpp PUBLIC ${yaml-cpp_SOURCE_DIR}/include)
  endif()

  set(${_out_target} yaml-cpp PARENT_SCOPE)
endfunction()

_yaze_ensure_yaml_cpp(YAZE_YAML_CPP_TARGET)

set(YAZE_AGENT_SOURCES
  cli/service/agent/proposal_executor.cc
  cli/handlers/agent/tool_commands.cc
  cli/handlers/agent/gui_tool_commands.cc
  cli/handlers/agent/todo_commands.cc
  cli/handlers/agent/hex_commands.cc
  cli/handlers/agent/palette_commands.cc
  cli/service/agent/conversational_agent_service.cc
  cli/service/agent/simple_chat_session.cc
  cli/service/agent/tool_dispatcher.cc
  cli/service/agent/learned_knowledge_service.cc
  cli/service/agent/todo_manager.cc
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
  cli/handlers/overworld_inspect.cc
  cli/handlers/message.cc
  cli/flags.cc
  cli/service/rom/rom_sandbox_manager.cc
)

if(YAZE_WITH_JSON)
  list(APPEND YAZE_AGENT_SOURCES cli/service/ai/gemini_ai_service.cc)
endif()

add_library(yaze_agent STATIC ${YAZE_AGENT_SOURCES})

set(_yaze_agent_link_targets
  yaze_common
  ${ABSL_TARGETS}
  ${YAZE_YAML_CPP_TARGET}
)

target_link_libraries(yaze_agent PUBLIC ${_yaze_agent_link_targets})

target_include_directories(yaze_agent
  PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/incl
    ${CMAKE_SOURCE_DIR}/third_party/httplib
    ${CMAKE_SOURCE_DIR}/third_party/json/include
    ${CMAKE_SOURCE_DIR}/src/lib
)

if(YAZE_YAML_CPP_TARGET)
  get_target_property(_yaze_yaml_include_dirs ${YAZE_YAML_CPP_TARGET} INTERFACE_INCLUDE_DIRECTORIES)
  if(_yaze_yaml_include_dirs)
    target_include_directories(yaze_agent PUBLIC ${_yaze_yaml_include_dirs})
  endif()
endif()

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
    ${PROJECT_SOURCE_DIR}/src/protos/canvas_automation.proto)
  
  target_link_libraries(yaze_agent PUBLIC
    grpc++
    grpc++_reflection
    libprotobuf
  )
  message(STATUS "✓ gRPC GUI automation enabled for yaze_agent")
endif()

set_target_properties(yaze_agent PROPERTIES POSITION_INDEPENDENT_CODE ON)
