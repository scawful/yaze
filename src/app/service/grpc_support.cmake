# ==============================================================================
# Yaze gRPC Support Library
# ==============================================================================
# This library consolidates ALL gRPC/protobuf usage to eliminate Windows
# linker errors (LNK1241, LNK2005). All protobuf definitions and gRPC
# service implementations are contained here, with other libraries linking
# to this single source of truth.
#
# Dependencies: yaze_util, yaze_common, yaze_app_core_lib, yaze_zelda3, yaze_gfx, yaze_gui
# ==============================================================================

set(
  YAZE_GRPC_SOURCES
  # Core gRPC services
  app/service/unified_grpc_server.cc
  app/service/canvas_automation_service.cc
  app/service/imgui_test_harness_service.cc
  app/service/widget_discovery_service.cc
  app/service/screenshot_utils.cc
  
  # Test infrastructure
  app/test/test_recorder.cc
  app/test/test_script_parser.cc
  
  # CLI agent gRPC client code (only files that actually exist)
  cli/service/planning/tile16_proposal_generator.cc
  cli/service/gui/gui_automation_client.cc
)

add_library(yaze_grpc_support STATIC ${YAZE_GRPC_SOURCES})

target_precompile_headers(yaze_grpc_support PRIVATE
  "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_SOURCE_DIR}/src/yaze_pch.h>"
)

target_include_directories(yaze_grpc_support PUBLIC
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/app
  ${CMAKE_SOURCE_DIR}/src/lib
  ${CMAKE_SOURCE_DIR}/src/lib/imgui
  ${CMAKE_SOURCE_DIR}/src/lib/imgui_test_engine
  ${CMAKE_SOURCE_DIR}/incl
  ${SDL2_INCLUDE_DIR}
  ${PROJECT_BINARY_DIR}
)

# Link all required yaze libraries
target_link_libraries(yaze_grpc_support PUBLIC
  yaze_app_core_lib
  yaze_util
  yaze_common
  yaze_zelda3
  yaze_gfx
  yaze_gui
  yaze_emulator
  ${ABSL_TARGETS}
  ${SDL_TARGETS}
)

# Add JSON support
if(YAZE_WITH_JSON)
  target_include_directories(yaze_grpc_support PUBLIC
    ${CMAKE_SOURCE_DIR}/third_party/json/include)
  target_compile_definitions(yaze_grpc_support PUBLIC YAZE_WITH_JSON)
endif()

# Add ALL protobuf definitions (consolidated from multiple libraries)
target_add_protobuf(yaze_grpc_support
  ${PROJECT_SOURCE_DIR}/src/protos/rom_service.proto
  ${PROJECT_SOURCE_DIR}/src/protos/canvas_automation.proto
  ${PROJECT_SOURCE_DIR}/src/protos/imgui_test_harness.proto
  ${PROJECT_SOURCE_DIR}/src/protos/emulator_service.proto
)

# Link gRPC and protobuf libraries (single point of linking)
target_link_libraries(yaze_grpc_support PUBLIC
  grpc++
  grpc++_reflection
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
