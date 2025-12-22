# ==============================================================================
# Yaze gRPC Support Library
# ==============================================================================
# This library consolidates ALL gRPC/protobuf usage to eliminate Windows
# linker errors (LNK1241, LNK2005). All protobuf definitions and gRPC
# service implementations are contained here, with other libraries linking
# to this single source of truth.
#
# Dependencies: yaze_util, yaze_common, yaze_zelda3, yaze_gfx, yaze_gui, yaze_emulator
# Note: yaze_app_core_lib is NOT a dependency to avoid dependency cycles
# ==============================================================================

set(
  YAZE_GRPC_SOURCES
  # Core gRPC services
  app/service/unified_grpc_server.cc
  app/service/canvas_automation_service.cc
  app/service/imgui_test_harness_service.cc
  app/service/widget_discovery_service.cc
  app/service/screenshot_utils.cc
  app/service/rom_service_impl.cc
  
  # Test infrastructure
  app/test/test_recorder.cc
  app/test/test_script_parser.cc
)

add_library(yaze_grpc_support STATIC ${YAZE_GRPC_SOURCES})

target_precompile_headers(yaze_grpc_support PRIVATE
  "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_SOURCE_DIR}/src/yaze_pch.h>"
)

target_include_directories(yaze_grpc_support PUBLIC
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/app
  ${CMAKE_SOURCE_DIR}/ext
  ${CMAKE_SOURCE_DIR}/ext/imgui
  ${CMAKE_SOURCE_DIR}/ext/imgui_test_engine
  ${CMAKE_SOURCE_DIR}/incl
  ${SDL2_INCLUDE_DIR}
  ${PROJECT_BINARY_DIR}
)

# Link required yaze libraries (avoid yaze_app_core_lib to break dependency cycle)
# The cycle was: yaze_agent -> yaze_grpc_support -> yaze_app_core_lib -> yaze_editor -> yaze_agent
target_link_libraries(yaze_grpc_support PUBLIC
  yaze_util
  yaze_common
  yaze_zelda3
  yaze_gfx
  yaze_gui
  yaze_emulator
  yaze_net
  ${ABSL_TARGETS}
  ${YAZE_SDL2_TARGETS}
)

# Add JSON support
if(YAZE_WITH_JSON)
  target_include_directories(yaze_grpc_support PUBLIC
    ${CMAKE_SOURCE_DIR}/ext/json/include)
  target_compile_definitions(yaze_grpc_support PUBLIC YAZE_WITH_JSON)
endif()

# Create a separate OBJECT library for proto files to break dependency cycles
# This allows yaze_agent to depend on the protos without depending on yaze_grpc_support
add_library(yaze_proto_gen OBJECT)
target_add_protobuf(yaze_proto_gen
  ${PROJECT_SOURCE_DIR}/src/protos/rom_service.proto
  ${PROJECT_SOURCE_DIR}/src/protos/canvas_automation.proto
  ${PROJECT_SOURCE_DIR}/src/protos/imgui_test_harness.proto
  ${PROJECT_SOURCE_DIR}/src/protos/emulator_service.proto
)

# Link proto gen to protobuf
target_link_libraries(yaze_proto_gen PUBLIC ${YAZE_PROTOBUF_TARGETS})

# Add proto objects to grpc_support
target_sources(yaze_grpc_support PRIVATE $<TARGET_OBJECTS:yaze_proto_gen>)
target_include_directories(yaze_grpc_support PUBLIC ${CMAKE_BINARY_DIR}/gens)

# Resolve gRPC targets (FetchContent builds expose bare names, vcpkg uses
# the gRPC:: namespace). Fallback gracefully.
set(_YAZE_GRPCPP_TARGET grpc++)
if(TARGET gRPC::grpc++)
  set(_YAZE_GRPCPP_TARGET gRPC::grpc++)
endif()

set(_YAZE_GRPCPP_REFLECTION_TARGET grpc++_reflection)
if(TARGET gRPC::grpc++_reflection)
  set(_YAZE_GRPCPP_REFLECTION_TARGET gRPC::grpc++_reflection)
endif()

if(NOT TARGET ${_YAZE_GRPCPP_TARGET})
  message(FATAL_ERROR "gRPC C++ target not available (checked ${_YAZE_GRPCPP_TARGET})")
endif()
if(NOT TARGET ${_YAZE_GRPCPP_REFLECTION_TARGET})
  message(FATAL_ERROR "gRPC reflection target not available (checked ${_YAZE_GRPCPP_REFLECTION_TARGET})")
endif()

# Link gRPC and protobuf libraries (single point of linking)
target_link_libraries(yaze_grpc_support PUBLIC
  ${_YAZE_GRPCPP_TARGET}
  ${_YAZE_GRPCPP_REFLECTION_TARGET}
  ${YAZE_PROTOBUF_TARGETS}
)

set_target_properties(yaze_grpc_support PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# Platform-specific compile definitions
if(UNIX AND NOT APPLE)
  target_compile_definitions(yaze_grpc_support PRIVATE linux stricmp=strcasecmp)
elseif(APPLE)
  target_compile_definitions(yaze_grpc_support PRIVATE MACOS)
elseif(WIN32)
  target_compile_definitions(yaze_grpc_support PRIVATE WINDOWS)
endif()

message(STATUS "✓ yaze_grpc_support library configured (consolidated gRPC/protobuf)")
