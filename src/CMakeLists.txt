set(
  YAZE_APP_EMU_SRC
  app/emu/audio/apu.cc
  app/emu/audio/audio_backend.cc
  app/emu/audio/spc700.cc
  app/emu/audio/dsp.cc
  app/emu/audio/internal/addressing.cc
  app/emu/audio/internal/instructions.cc
  app/emu/cpu/internal/addressing.cc
  app/emu/cpu/internal/instructions.cc
  app/emu/debug/apu_debugger.cc
  app/emu/debug/disassembly_viewer.cc
  app/emu/debug/breakpoint_manager.cc
  app/emu/debug/watchpoint_manager.cc
  app/emu/input/input_backend.cc
  app/emu/input/input_manager.cc
  app/emu/ui/input_handler.cc
  app/emu/ui/emulator_ui.cc
  app/emu/ui/debugger_ui.cc
  app/emu/cpu/cpu.cc
  app/emu/video/ppu.cc
  app/emu/memory/dma.cc
  app/emu/memory/memory.cc
  app/emu/snes.cc
)

set(YAZE_RESOURCE_FILES
  ${CMAKE_SOURCE_DIR}/assets/font/Karla-Regular.ttf
  ${CMAKE_SOURCE_DIR}/assets/font/Roboto-Medium.ttf
  ${CMAKE_SOURCE_DIR}/assets/font/Cousine-Regular.ttf
  ${CMAKE_SOURCE_DIR}/assets/font/DroidSans.ttf
  ${CMAKE_SOURCE_DIR}/assets/font/NotoSansJP.ttf
  ${CMAKE_SOURCE_DIR}/assets/font/IBMPlexSansJP-Bold.ttf
  ${CMAKE_SOURCE_DIR}/assets/font/MaterialIcons-Regular.ttf
)

# Add theme files for macOS bundle (replacing the glob pattern with explicit files)
file(GLOB YAZE_THEME_FILES "${CMAKE_SOURCE_DIR}/assets/themes/*.theme")
list(APPEND YAZE_RESOURCE_FILES ${YAZE_THEME_FILES})

# Configure assets for different platforms
foreach (FILE ${YAZE_RESOURCE_FILES})
  file(RELATIVE_PATH NEW_FILE "${CMAKE_SOURCE_DIR}/assets" ${FILE})
  get_filename_component(NEW_FILE_PATH ${NEW_FILE} DIRECTORY)
  
  if (APPLE)
    # macOS: Set bundle location
    set_source_files_properties(${FILE}
      PROPERTIES
      MACOSX_PACKAGE_LOCATION "Resources/${NEW_FILE_PATH}"
    )
  else()
    # Windows/Linux: Copy to output directory
    set_source_files_properties(${FILE}
      PROPERTIES
      VS_DEPLOYMENT_CONTENT 1
      VS_DEPLOYMENT_LOCATION "assets/${NEW_FILE_PATH}"
    )
  endif()
endforeach()

# Conditionally add native file dialog (optional for CI builds)
if(NOT YAZE_MINIMAL_BUILD)
  # Check if we can build NFD before adding it
  find_package(PkgConfig QUIET)
  if(PKG_CONFIG_FOUND AND UNIX AND NOT APPLE)
    pkg_check_modules(GTK3 QUIET gtk+-3.0)
    if(GTK3_FOUND)
      add_subdirectory(lib/nativefiledialog-extended)
      set(YAZE_HAS_NFD ON)
      message(STATUS "NFD enabled with GTK3 support")
    else()
      set(YAZE_HAS_NFD OFF)
      message(STATUS "NFD disabled - GTK3 not found")
    endif()
  elseif(WIN32 OR APPLE)
    add_subdirectory(lib/nativefiledialog-extended)
    set(YAZE_HAS_NFD ON)
    message(STATUS "NFD enabled for Windows/macOS")
  else()
    set(YAZE_HAS_NFD OFF)
    message(STATUS "NFD disabled - no platform support")
  endif()
else()
  set(YAZE_HAS_NFD OFF)
  message(STATUS "NFD disabled for minimal build")
endif()

if(YAZE_BUILD_APP OR YAZE_BUILD_Z3ED)
  include(cli/agent.cmake)
endif()

  message(STATUS "Using modular build system")

  if(YAZE_BUILD_LIB OR YAZE_BUILD_APP OR YAZE_BUILD_Z3ED)
    include(util/util.cmake)
    include(app/core/core_library.cmake)
    include(app/gfx/gfx_library.cmake)
    include(app/gui/gui_library.cmake)
    include(app/net/net_library.cmake)
    include(app/zelda3/zelda3_library.cmake)
    include(app/editor/editor_library.cmake)

    if(YAZE_BUILD_TESTS AND NOT YAZE_MINIMAL_BUILD)
      include(app/test/test.cmake)
    endif()

    if(YAZE_BUILD_EMU)
      include(app/emu/emu_library.cmake)
    endif()
  endif()

if (YAZE_BUILD_APP)
  if (APPLE)
    add_executable(
      yaze
      MACOSX_BUNDLE
      app/main.cc
      # Bundled Resources
      ${YAZE_RESOURCE_FILES}
    )

    set(ICON_FILE "${CMAKE_SOURCE_DIR}/assets/yaze.icns")
    target_sources(yaze PRIVATE ${ICON_FILE})
    set_source_files_properties(${ICON_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

    set_target_properties(yaze PROPERTIES
      MACOSX_BUNDLE_ICON_FILE "yaze.icns"
      MACOSX_BUNDLE_BUNDLE_NAME "Yaze"
      MACOSX_BUNDLE_EXECUTABLE_NAME "yaze"
      MACOSX_BUNDLE_GUI_IDENTIFIER "com.scawful.yaze"
      MACOSX_BUNDLE_INFO_STRING "Yet Another Zelda3 Editor"
      MACOSX_BUNDLE_LONG_VERSION_STRING "${PROJECT_VERSION}"
      MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
      MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
      MACOSX_BUNDLE_COPYRIGHT "Copyright © 2024 scawful. All rights reserved."
    )
  else()
    add_executable(
      yaze
      app/main.cc
    )

    if(WIN32 OR UNIX)
      target_sources(yaze PRIVATE ${YAZE_RESOURCE_FILES})

      if(WIN32)
        foreach(ASSET_FILE ${YAZE_RESOURCE_FILES})
          file(RELATIVE_PATH ASSET_REL_PATH "${CMAKE_SOURCE_DIR}/assets" ${ASSET_FILE})
          get_filename_component(ASSET_DIR ${ASSET_REL_PATH} DIRECTORY)

          set_source_files_properties(${ASSET_FILE}
            PROPERTIES
            VS_DEPLOYMENT_CONTENT 1
            VS_DEPLOYMENT_LOCATION "assets/${ASSET_DIR}"
          )
        endforeach()
      endif()
    endif()
  endif()

  target_include_directories(
    yaze PUBLIC
    ${CMAKE_SOURCE_DIR}/src/lib/
    ${CMAKE_SOURCE_DIR}/src/app/
    ${CMAKE_SOURCE_DIR}/src/lib/asar/src
    ${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar
    ${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar-dll-bindings/c
    ${CMAKE_SOURCE_DIR}/incl/
    ${CMAKE_SOURCE_DIR}/src/
    ${CMAKE_SOURCE_DIR}/src/lib/imgui_test_engine
    ${CMAKE_SOURCE_DIR}/third_party/httplib
    ${SDL2_INCLUDE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${PROJECT_BINARY_DIR}
  )

  target_sources(yaze PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/yaze_config.h)

  set_source_files_properties(
    ${CMAKE_CURRENT_BINARY_DIR}/yaze_config.h
    PROPERTIES GENERATED TRUE
  )

  source_group(TREE ${CMAKE_CURRENT_BINARY_DIR}
               FILES ${CMAKE_CURRENT_BINARY_DIR}/yaze_config.h)

  if(PNG_FOUND)
    target_include_directories(yaze PUBLIC ${PNG_INCLUDE_DIRS})
  endif()

  if(YAZE_HAS_NFD)
    target_link_libraries(yaze PRIVATE nfd)
    target_compile_definitions(yaze PRIVATE YAZE_ENABLE_NFD=1)
  else()
    target_compile_definitions(yaze PRIVATE YAZE_ENABLE_NFD=0)
  endif()

  if(YAZE_USE_MODULAR_BUILD)
    set(_yaze_modular_links yaze_editor)

    if(TARGET yaze_agent)
      list(APPEND _yaze_modular_links yaze_agent)
    endif()

    if(YAZE_BUILD_EMU AND TARGET yaze_emulator)
      list(APPEND _yaze_modular_links yaze_emulator)
    endif()

    if(YAZE_BUILD_TESTS AND TARGET yaze_test_support)
      list(APPEND _yaze_modular_links yaze_test_support)
    endif()

    target_link_libraries(yaze PRIVATE ${_yaze_modular_links})
  else()
    target_link_libraries(yaze PRIVATE yaze_core)
  endif()

  target_compile_definitions(yaze PRIVATE YAZE_ENABLE_POLICY_FRAMEWORK=1)

  if(WIN32)
    if(MSVC)
      target_link_options(yaze PRIVATE
        /STACK:8388608
        /SUBSYSTEM:WINDOWS
        /ENTRY:mainCRTStartup
      )
      message(STATUS "Configuring yaze as Windows GUI application with main() entry point")
    elseif(MINGW OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      target_link_options(yaze PRIVATE
        -Wl,--stack,8388608
        -Wl,--subsystem,windows
        -Wl,-emain
      )
      message(STATUS "Configuring yaze as Windows GUI application with main() entry point (MinGW)")
    endif()
  endif()

  if(YAZE_ENABLE_UI_TESTS)
    if(TARGET ImGuiTestEngine)
      target_include_directories(yaze PUBLIC ${CMAKE_SOURCE_DIR}/src/lib/imgui_test_engine)
      target_link_libraries(yaze PUBLIC ImGuiTestEngine)
      target_compile_definitions(yaze PRIVATE
        YAZE_ENABLE_IMGUI_TEST_ENGINE=1
        ${IMGUI_TEST_ENGINE_DEFINITIONS})
    else()
      target_compile_definitions(yaze PRIVATE YAZE_ENABLE_IMGUI_TEST_ENGINE=0)
    endif()
  else()
    target_compile_definitions(yaze PRIVATE YAZE_ENABLE_IMGUI_TEST_ENGINE=0)
  endif()

  if(YAZE_BUILD_TESTS AND TARGET gtest)
    target_link_libraries(yaze PRIVATE gtest)
    target_compile_definitions(yaze PRIVATE YAZE_ENABLE_GTEST=1)
    target_compile_definitions(yaze PRIVATE YAZE_ENABLE_TESTING=1)
  else()
    target_compile_definitions(yaze PRIVATE YAZE_ENABLE_GTEST=0)
    target_compile_definitions(yaze PRIVATE YAZE_ENABLE_TESTING=0)
  endif()

  if(PNG_FOUND)
    target_link_libraries(yaze PUBLIC ${PNG_LIBRARIES})
  endif()

  if (APPLE)
    target_link_libraries(yaze PUBLIC ${COCOA_LIBRARY})
  endif()

  if(NOT APPLE)
    add_custom_command(TARGET yaze POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory
      $<TARGET_FILE_DIR:yaze>/assets/font
      COMMAND ${CMAKE_COMMAND} -E copy_directory
      ${CMAKE_SOURCE_DIR}/assets/font
      $<TARGET_FILE_DIR:yaze>/assets/font
      COMMENT "Copying font assets"
    )

    add_custom_command(TARGET yaze POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory
      $<TARGET_FILE_DIR:yaze>/assets/themes
      COMMAND ${CMAKE_COMMAND} -E copy_directory
      ${CMAKE_SOURCE_DIR}/assets/themes
      $<TARGET_FILE_DIR:yaze>/assets/themes
      COMMENT "Copying theme assets"
    )

    if(EXISTS ${CMAKE_SOURCE_DIR}/assets/layouts)
      add_custom_command(TARGET yaze POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:yaze>/assets/layouts
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets/layouts
        $<TARGET_FILE_DIR:yaze>/assets/layouts
        COMMENT "Copying layout assets"
      )
    endif()

    if(EXISTS ${CMAKE_SOURCE_DIR}/assets/lib)
      add_custom_command(TARGET yaze POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:yaze>/assets/lib
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets/lib
        $<TARGET_FILE_DIR:yaze>/assets/lib
        COMMENT "Copying library assets"
      )
    endif()
  endif()

  if(YAZE_WITH_GRPC)
    message(STATUS "Adding gRPC ImGuiTestHarness to yaze target")

    target_include_directories(yaze PRIVATE
      ${CMAKE_SOURCE_DIR}/third_party/json/include)
    target_compile_definitions(yaze PRIVATE YAZE_WITH_JSON)

    if(NOT YAZE_USE_MODULAR_BUILD)
      target_add_protobuf(yaze
        ${CMAKE_SOURCE_DIR}/src/protos/imgui_test_harness.proto
        ${CMAKE_SOURCE_DIR}/src/protos/canvas_automation.proto)

      target_sources(yaze PRIVATE
        ${CMAKE_SOURCE_DIR}/src/app/core/service/imgui_test_harness_service.cc
        ${CMAKE_SOURCE_DIR}/src/app/core/service/imgui_test_harness_service.h
        ${CMAKE_SOURCE_DIR}/src/app/core/service/screenshot_utils.cc
        ${CMAKE_SOURCE_DIR}/src/app/core/service/screenshot_utils.h
        ${CMAKE_SOURCE_DIR}/src/app/core/service/widget_discovery_service.cc
        ${CMAKE_SOURCE_DIR}/src/app/core/service/widget_discovery_service.h
        ${CMAKE_SOURCE_DIR}/src/app/core/testing/test_recorder.cc
        ${CMAKE_SOURCE_DIR}/src/app/core/testing/test_recorder.h
        ${CMAKE_SOURCE_DIR}/src/app/core/testing/test_script_parser.cc
        ${CMAKE_SOURCE_DIR}/src/app/core/testing/test_script_parser.h)
    endif()

    target_link_libraries(yaze PRIVATE
      grpc++
      grpc++_reflection
      libprotobuf)

    message(STATUS "✓ gRPC ImGuiTestHarness integrated")
    message(STATUS "✓ AI Agent services integrated into yaze GUI")
  endif()
endif()

if(YAZE_BUILD_EMU)
  if (NOT YAZE_MINIMAL_BUILD AND APPLE)
    add_executable(
      yaze_emu
      MACOSX_BUNDLE
      app/emu/emu.cc
      app/rom.cc
      app/platform/app_delegate.mm
      ${YAZE_APP_EMU_SRC}
      ${YAZE_APP_CORE_SRC}
      ${YAZE_APP_EDITOR_SRC}
      ${YAZE_APP_GFX_SRC}
      ${YAZE_APP_ZELDA3_SRC}
      ${YAZE_UTIL_SRC}
      ${YAZE_GUI_SRC}
      ${IMGUI_SRC}
      cli/service/planning/proposal_registry.cc
      cli/service/rom/rom_sandbox_manager.cc
    )
    target_link_libraries(yaze_emu PUBLIC ${COCOA_LIBRARY})
    target_compile_definitions(yaze_emu PRIVATE IMGUI_DEFINE_MATH_OPERATORS)
  elseif(NOT YAZE_MINIMAL_BUILD)
    add_executable(
      yaze_emu
      app/rom.cc
      app/emu/emu.cc
      ${YAZE_APP_EMU_SRC}
      ${YAZE_APP_CORE_SRC}
      ${YAZE_APP_EDITOR_SRC}
      ${YAZE_APP_GFX_SRC}
      ${YAZE_APP_ZELDA3_SRC}
      ${YAZE_UTIL_SRC}
      ${YAZE_GUI_SRC}
      ${IMGUI_SRC}
      cli/service/planning/proposal_registry.cc
      cli/service/rom/rom_sandbox_manager.cc
    )
  endif()

  if(NOT YAZE_MINIMAL_BUILD)
    target_include_directories(
      yaze_emu PUBLIC
      ${CMAKE_SOURCE_DIR}/src/lib/
      ${CMAKE_SOURCE_DIR}/src/app/
      ${CMAKE_SOURCE_DIR}/src/lib/asar/src
      ${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar
      ${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar-dll-bindings/c
      ${CMAKE_SOURCE_DIR}/incl/
      ${CMAKE_SOURCE_DIR}/src/
      ${CMAKE_SOURCE_DIR}/src/lib/imgui_test_engine
      ${CMAKE_SOURCE_DIR}/third_party/httplib
      ${PNG_INCLUDE_DIRS}
      ${SDL2_INCLUDE_DIR}
      ${CMAKE_CURRENT_BINARY_DIR}
      ${PROJECT_BINARY_DIR}
    )

target_link_libraries(yaze_emu PUBLIC
      ${ABSL_TARGETS}
      ${SDL_TARGETS}
      ${PNG_LIBRARIES}
      ${CMAKE_DL_LIBS}
      ImGui
      asar-static
      nlohmann_json::nlohmann_json
      yaze_agent
      yaze_editor
      yaze_zelda3
      yaze_gui
      yaze_gfx
      yaze_core_lib
      yaze_util
      yaze_test_support
    )

    if(YAZE_WITH_GRPC)
      target_compile_definitions(yaze_emu PRIVATE YAZE_WITH_JSON)
      target_include_directories(yaze_emu PRIVATE ${CMAKE_SOURCE_DIR}/third_party/json/include)

      target_add_protobuf(yaze_emu
        ${CMAKE_SOURCE_DIR}/src/protos/imgui_test_harness.proto
        ${CMAKE_SOURCE_DIR}/src/protos/canvas_automation.proto)

      target_link_libraries(yaze_emu PRIVATE
        grpc++
        grpc++_reflection
        libprotobuf)
    endif()
  endif()
endif()
if (YAZE_BUILD_Z3ED)
  include(cli/z3ed.cmake)
endif()

if(MACOS)
  set_target_properties(yaze
    PROPERTIES
    BUNDLE True
    OUTPUT_NAME "yaze"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/cmake/yaze.plist.in
    RESOURCE ${YAZE_RESOURCE_FILES}
  )
elseif(UNIX)
  set_target_properties(yaze
    PROPERTIES
    BUNDLE True
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  )
  target_compile_definitions(yaze PRIVATE "linux")
  target_compile_definitions(yaze PRIVATE "stricmp=strcasecmp")
else()
  if(YAZE_MINIMAL_BUILD)
    # Skip Windows resource file in CI/minimal builds to avoid architecture conflicts
    set_target_properties(yaze
      PROPERTIES
      ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
      LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
  else()
    # Windows resource file - only for x64 architecture
    set_target_properties(yaze
      PROPERTIES
      ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
      LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    
    # Only use resource file for x64 Windows builds
    if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
      # Generate yaze.res from yaze.rc and yaze.ico for Windows x64 builds
      set(YAZE_RC_FILE "${CMAKE_CURRENT_SOURCE_DIR}/app/platform/win32/yaze.rc")
      set(YAZE_ICO_FILE "${CMAKE_SOURCE_DIR}/assets/yaze.ico")
      set(YAZE_RES_FILE "${CMAKE_CURRENT_BINARY_DIR}/yaze.res")

      # Add a custom command to generate the .res file from the .rc and .ico
      # /I adds include directory so RC compiler can find yaze.ico
      add_custom_command(
        OUTPUT ${YAZE_RES_FILE}
        COMMAND ${CMAKE_RC_COMPILER} /fo ${YAZE_RES_FILE} /I "${CMAKE_SOURCE_DIR}/assets" ${YAZE_RC_FILE}
        DEPENDS ${YAZE_RC_FILE} ${YAZE_ICO_FILE}
        COMMENT "Generating yaze.res from yaze.rc and yaze.ico"
        VERBATIM
      )

      # Make sure the resource file is built before linking
      add_custom_target(yaze_res ALL DEPENDS ${YAZE_RES_FILE})

      # Link the generated .res file to the yaze target
      target_sources(yaze PRIVATE ${YAZE_RES_FILE})
    endif()
  endif()
endif()

# Yaze Core Library (for testing and C API)
if (YAZE_BUILD_LIB)
  # Sources shared by the C API library (minimal dependencies)
  set(YAZE_C_SOURCES
    ./yaze.cc
    cli/service/gui/gui_automation_client.cc
  )

  if(YAZE_USE_MODULAR_BUILD)
    # Aggregate modular libraries into an interface target for backward compatibility
    if(NOT TARGET yaze_core)
      add_library(yaze_core INTERFACE)
    endif()

    target_include_directories(
      yaze_core INTERFACE
      ${CMAKE_SOURCE_DIR}/src/lib/
      ${CMAKE_SOURCE_DIR}/src/app/
      ${CMAKE_SOURCE_DIR}/src/lib/asar/src
      ${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar
      ${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar-dll-bindings/c
      ${CMAKE_SOURCE_DIR}/incl/
      ${CMAKE_SOURCE_DIR}/src/
      ${CMAKE_SOURCE_DIR}/src/lib/imgui
      ${CMAKE_SOURCE_DIR}/src/lib/imgui_test_engine
      ${SDL2_INCLUDE_DIR}
      ${PROJECT_BINARY_DIR}
    )

    target_link_libraries(
      yaze_core INTERFACE
      yaze_util
      yaze_gfx
      yaze_gui
      yaze_zelda3
      yaze_core_lib
      yaze_editor
      ImGui
    )

    if(TARGET yaze_agent)
      target_link_libraries(yaze_core INTERFACE yaze_agent)
    endif()

    if(YAZE_BUILD_EMU AND TARGET yaze_emulator)
      target_link_libraries(yaze_core INTERFACE yaze_emulator)
    endif()

    if(YAZE_ENABLE_UI_TESTS AND TARGET ImGuiTestEngine)
      target_link_libraries(yaze_core INTERFACE ImGuiTestEngine)
    endif()

    if(YAZE_WITH_GRPC)
      target_link_libraries(yaze_core INTERFACE
        grpc++
        grpc++_reflection
        libprotobuf)
    endif()
  else()
    # Create core library for testing (includes editor and zelda3 components needed by tests)
    include(util/util.cmake)
    set(YAZE_CORE_SOURCES
      app/rom.cc
      ${YAZE_APP_CORE_SRC}
      ${YAZE_APP_GFX_SRC} 
      ${YAZE_APP_EDITOR_SRC}
      ${YAZE_APP_ZELDA3_SRC}
      ${YAZE_APP_EMU_SRC}
      ${YAZE_GUI_SRC}
      # cli/service/gui_automation_client.cc # Moved to yaze_c
      cli/service/testing/test_workflow_generator.cc
    )

    add_library(yaze_core STATIC ${YAZE_CORE_SOURCES})

    target_include_directories(
      yaze_core PUBLIC
      ${CMAKE_SOURCE_DIR}/src/lib/
      ${CMAKE_SOURCE_DIR}/src/app/
      ${CMAKE_SOURCE_DIR}/src/lib/asar/src
      ${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar
      ${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar-dll-bindings/c
      ${CMAKE_SOURCE_DIR}/incl/
      ${CMAKE_SOURCE_DIR}/src/
      ${CMAKE_SOURCE_DIR}/src/lib/imgui
      ${CMAKE_SOURCE_DIR}/src/lib/imgui_test_engine
      ${SDL2_INCLUDE_DIR}
      ${PROJECT_BINARY_DIR}
      ${CMAKE_CURRENT_BINARY_DIR}
    )

    target_link_libraries(
      yaze_core PUBLIC
      asar-static
      yaze_agent
      yaze_util
      ${ABSL_TARGETS}
      ${SDL_TARGETS}
      ${CMAKE_DL_LIBS}
      ImGui
    )

    if(YAZE_WITH_GRPC)
      target_add_protobuf(yaze_core 
        ${CMAKE_SOURCE_DIR}/src/protos/imgui_test_harness.proto
        ${CMAKE_SOURCE_DIR}/src/protos/canvas_automation.proto)

      target_link_libraries(yaze_core PRIVATE
        grpc++
        grpc++_reflection
        libprotobuf)
    endif()
  endif()

  # Create the full C API library (static for CI, shared for release)
  if(YAZE_MINIMAL_BUILD)
    add_library(yaze_c STATIC ${YAZE_C_SOURCES})
  else()
    add_library(yaze_c SHARED ${YAZE_C_SOURCES})
  endif()

  target_include_directories(
    yaze_c PUBLIC
    ${CMAKE_SOURCE_DIR}/src/lib/
    ${CMAKE_SOURCE_DIR}/src/app/
    ${CMAKE_SOURCE_DIR}/src/lib/asar/src
    ${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar
    ${CMAKE_SOURCE_DIR}/src/lib/asar/src/asar-dll-bindings/c
    ${CMAKE_SOURCE_DIR}/incl/
    ${CMAKE_SOURCE_DIR}/src/
    ${CMAKE_SOURCE_DIR}/src/lib/imgui_test_engine
    ${SDL2_INCLUDE_DIR}
    ${PROJECT_BINARY_DIR}
  )

  if(PNG_FOUND)
    target_include_directories(yaze_c PUBLIC ${PNG_INCLUDE_DIRS})
    if(NOT YAZE_USE_MODULAR_BUILD)
      target_include_directories(yaze_core PUBLIC ${PNG_INCLUDE_DIRS})
    endif()
  endif()

  if(YAZE_USE_MODULAR_BUILD)
    target_link_libraries(
      yaze_c PRIVATE
      yaze_util
      yaze_gfx
      yaze_gui
      yaze_zelda3
      yaze_core_lib
      yaze_editor
      ImGui
    )

    if(TARGET yaze_agent)
      target_link_libraries(yaze_c PRIVATE yaze_agent)
    endif()

    if(YAZE_BUILD_EMU AND TARGET yaze_emulator)
      target_link_libraries(yaze_c PRIVATE yaze_emulator)
    endif()
  else()
    target_link_libraries(
      yaze_c PRIVATE
      yaze_core
      ImGui
    )
  endif()

  if(YAZE_WITH_GRPC)
    target_add_protobuf(yaze_c 
      ${CMAKE_SOURCE_DIR}/src/protos/imgui_test_harness.proto
      ${CMAKE_SOURCE_DIR}/src/protos/canvas_automation.proto)

    target_link_libraries(yaze_c PRIVATE
      grpc++
      grpc++_reflection
      libprotobuf)
  endif()

  if(YAZE_ENABLE_UI_TESTS AND TARGET ImGuiTestEngine)
    target_link_libraries(yaze_c PRIVATE ImGuiTestEngine)
    target_compile_definitions(yaze_c PRIVATE YAZE_ENABLE_IMGUI_TEST_ENGINE=1)
  else()
    target_compile_definitions(yaze_c PRIVATE YAZE_ENABLE_IMGUI_TEST_ENGINE=0)
  endif()

  # Link with test support library if available (required by editor)
  if(TARGET yaze_test_support)
    target_link_libraries(yaze_c PRIVATE yaze_test_support)
  endif()

  if(PNG_FOUND)
    target_link_libraries(yaze_c PRIVATE ${PNG_LIBRARIES})
    if(NOT YAZE_USE_MODULAR_BUILD)
      target_link_libraries(yaze_core PRIVATE ${PNG_LIBRARIES})
    endif()
  endif()

  if (YAZE_INSTALL_LIB)
    install(TARGETS yaze_c
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib/static)

    install(
      FILES
        incl/yaze.h
        incl/zelda.h
      DESTINATION
        include
    )
  endif()
endif()

# =============================================================================
# Visual Studio Source Groups Organization
# =============================================================================
# These source groups will organize files in Visual Studio Solution Explorer
# into logical, hierarchical folders for better navigation and development.

# Core Application Structure
source_group("Application\\Core" FILES
  app/main.cc
  app/rom.cc
  app/rom.h
  app/snes.h
  app/transaction.h
  yaze.cc
)

# App Core Components
source_group("Application\\Core\\Controller" FILES
  app/core/controller.cc
  app/core/controller.h
  app/core/window.cc
  app/core/window.h
)

source_group("Application\\Core\\Project" FILES
  app/core/project.cc
  app/core/project.h
  app/core/features.h
)

source_group("Application\\Core\\Asar" FILES
  app/core/asar_wrapper.cc
  app/core/asar_wrapper.h
)

# Platform-specific files
source_group("Application\\Core\\Platform" FILES
  app/platform/app_delegate.h
  app/platform/app_delegate.mm
  app/platform/clipboard.cc
  app/platform/clipboard.h
  app/platform/clipboard.mm
  app/platform/file_dialog.cc
  app/platform/file_dialog.h
  app/platform/file_dialog.mm
  app/platform/font_loader.cc
  app/platform/font_loader.h
  app/platform/font_loader.mm
  app/platform/view_controller.h
)

# Editor System
source_group("Application\\Editor" FILES
  app/editor/editor_manager.cc
  app/editor/editor_manager.h
  app/editor/editor.h
  app/editor/editor_safeguards.h
)

# Code Editor
source_group("Application\\Editor\\Code" FILES
  app/editor/code/assembly_editor.cc
  app/editor/code/assembly_editor.h
  app/editor/code/memory_editor.h
)

# Dungeon Editor
source_group("Application\\Editor\\Dungeon" FILES
  app/editor/dungeon/dungeon_editor.cc
  app/editor/dungeon/dungeon_editor.h
  app/editor/dungeon/dungeon_map_editor.cc
  app/editor/dungeon/dungeon_map_editor.h
  app/editor/dungeon/dungeon_room_editor.cc
  app/editor/dungeon/dungeon_room_editor.h
  app/editor/dungeon/dungeon_sprite_editor.cc
  app/editor/dungeon/dungeon_sprite_editor.h
  app/editor/dungeon/dungeon_tile_editor.cc
  app/editor/dungeon/dungeon_tile_editor.h
  app/editor/dungeon/room_data_editor.cc
  app/editor/dungeon/room_data_editor.h
  app/editor/dungeon/room_properties_editor.cc
  app/editor/dungeon/room_properties_editor.h
  app/editor/dungeon/room_sprite_editor.cc
  app/editor/dungeon/room_sprite_editor.h
  app/editor/dungeon/room_tile_editor.cc
  app/editor/dungeon/room_tile_editor.h
)

# Graphics Editor
source_group("Application\\Editor\\Graphics" FILES
  app/editor/graphics/graphics_editor.cc
  app/editor/graphics/graphics_editor.h
  app/editor/graphics/palette_editor.cc
  app/editor/graphics/palette_editor.h
  app/editor/graphics/sprite_editor.cc
  app/editor/graphics/sprite_editor.h
  app/editor/graphics/tile_editor.cc
  app/editor/graphics/tile_editor.h
)

# Message Editor
source_group("Application\\Editor\\Message" FILES
  app/editor/message/message_editor.cc
  app/editor/message/message_editor.h
  app/editor/message/text_editor.cc
  app/editor/message/text_editor.h
  app/editor/message/translation_editor.cc
  app/editor/message/translation_editor.h
)

# Music Editor
source_group("Application\\Editor\\Music" FILES
  app/editor/music/music_editor.cc
  app/editor/music/music_editor.h
)

# Overworld Editor
source_group("Application\\Editor\\Overworld" FILES
  app/editor/overworld/overworld_editor.cc
  app/editor/overworld/overworld_editor.h
  app/editor/overworld/overworld_map_editor.cc
  app/editor/overworld/overworld_map_editor.h
  app/editor/overworld/overworld_sprite_editor.cc
  app/editor/overworld/overworld_sprite_editor.h
  app/editor/overworld/overworld_tile_editor.cc
  app/editor/overworld/overworld_tile_editor.h
  app/editor/overworld/overworld_transport_editor.cc
  app/editor/overworld/overworld_transport_editor.h
  app/editor/overworld/overworld_entrance_editor.cc
  app/editor/overworld/overworld_entrance_editor.h
)

# Sprite Editor
source_group("Application\\Editor\\Sprite" FILES
  app/editor/sprite/sprite_editor.cc
  app/editor/sprite/sprite_editor.h
  app/editor/sprite/sprite_properties_editor.cc
  app/editor/sprite/sprite_properties_editor.h
)

# System Editor
source_group("Application\\Editor\\System" FILES
  app/editor/system/asm_editor.cc
  app/editor/system/asm_editor.h
  app/editor/system/config_editor.cc
  app/editor/system/config_editor.h
  app/editor/system/debug_console.cc
  app/editor/system/debug_console.h
  app/editor/system/hex_editor.cc
  app/editor/system/hex_editor.h
  app/editor/system/log_viewer.cc
  app/editor/system/log_viewer.h
  app/editor/system/memory_editor.cc
  app/editor/system/memory_editor.h
  app/editor/system/proposal_drawer.cc
  app/editor/system/proposal_drawer.h
  app/editor/system/rom_analyzer.cc
  app/editor/system/rom_analyzer.h
)

# Emulator
source_group("Application\\Emulator" FILES
  app/emu/emu.cc
  app/emu/emulator.cc
  app/emu/emulator.h
  app/emu/snes.cc
  app/emu/snes.h
)

# Audio System
source_group("Application\\Emulator\\Audio" FILES
  app/emu/audio/apu.cc
  app/emu/audio/apu.h
  app/emu/audio/audio_backend.cc
  app/emu/audio/audio_backend.h
  app/emu/audio/spc700.cc
  app/emu/audio/spc700.h
  app/emu/audio/dsp.cc
  app/emu/audio/dsp.h
  app/emu/audio/internal/addressing.cc
  app/emu/audio/internal/addressing.h
  app/emu/audio/internal/instructions.cc
  app/emu/audio/internal/instructions.h
)

# CPU System
source_group("Application\\Emulator\\CPU" FILES
  app/emu/cpu/cpu.cc
  app/emu/cpu/cpu.h
  app/emu/cpu/internal/addressing.cc
  app/emu/cpu/internal/addressing.h
  app/emu/cpu/internal/instructions.cc
  app/emu/cpu/internal/instructions.h
)

# Memory System
source_group("Application\\Emulator\\Memory" FILES
  app/emu/memory/dma.cc
  app/emu/memory/dma.h
  app/emu/memory/memory.cc
  app/emu/memory/memory.h
  app/emu/memory/mock_memory.h
)

# Video System
source_group("Application\\Emulator\\Video" FILES
  app/emu/video/ppu.cc
  app/emu/video/ppu.h
  app/emu/video/ppu_registers.h
)

# Input System (abstracted for SDL2/SDL3)
source_group("Application\\Emulator\\Input" FILES
  app/emu/input/input_backend.cc
  app/emu/input/input_backend.h
  app/emu/input/input_manager.cc
  app/emu/input/input_manager.h
)

# Emulator UI System
source_group("Application\\Emulator\\UI" FILES
  app/emu/ui/input_handler.cc
  app/emu/ui/input_handler.h
  app/emu/ui/emulator_ui.cc
  app/emu/ui/emulator_ui.h
  app/emu/ui/debugger_ui.cc
  app/emu/ui/debugger_ui.h
)

# Debug System
source_group("Application\\Emulator\\Debug" FILES
  app/emu/debug/apu_debugger.cc
  app/emu/debug/apu_debugger.h
  app/emu/debug/breakpoint_manager.cc
  app/emu/debug/breakpoint_manager.h
  app/emu/debug/disassembly_viewer.cc
  app/emu/debug/disassembly_viewer.h
  app/emu/debug/watchpoint_manager.cc
  app/emu/debug/watchpoint_manager.h
)

# Graphics System
source_group("Application\\Graphics" FILES
  app/gfx/arena.cc
  app/gfx/arena.h
  app/gfx/background_buffer.cc
  app/gfx/background_buffer.h
  app/gfx/bitmap.cc
  app/gfx/bitmap.h
  app/gfx/compression.cc
  app/gfx/compression.h
  app/gfx/performance_profiler.cc
  app/gfx/performance_profiler.h
  app/gfx/scad_format.cc
  app/gfx/scad_format.h
  app/gfx/snes_color.cc
  app/gfx/snes_color.h
  app/gfx/snes_palette.cc
  app/gfx/snes_palette.h
  app/gfx/snes_tile.cc
  app/gfx/snes_tile.h
  app/gfx/tilemap.cc
  app/gfx/tilemap.h
)

# GUI System
source_group("Application\\GUI" FILES
  app/gui/canvas_utils.cc
  app/gui/canvas_utils.h
  app/gui/canvas.cc
  app/gui/canvas.h
  app/gui/color.cc
  app/gui/color.h
  app/gui/enhanced_palette_editor.cc
  app/gui/widgets/palette_widget.h
  app/gui/icons.h
  app/gui/input.cc
  app/gui/input.h
  app/gui/style.cc
  app/gui/style.h
  app/gui/theme_manager.cc
  app/gui/theme_manager.h
  app/gui/zeml.cc
  app/gui/zeml.h
)

# GUI Modules
source_group("Application\\GUI\\Modules" FILES
  app/gui/modules/about_dialog.cc
  app/gui/modules/about_dialog.h
  app/gui/modules/preferences_dialog.cc
  app/gui/modules/preferences_dialog.h
  app/gui/modules/project_dialog.cc
  app/gui/modules/project_dialog.h
)

# Zelda3 Specific
source_group("Application\\Zelda3" FILES
  app/zelda3/common.h
  app/zelda3/hyrule_magic.cc
  app/zelda3/hyrule_magic.h
)

# Zelda3 Dungeon
source_group("Application\\Zelda3\\Dungeon" FILES
  app/zelda3/dungeon/dungeon_data.cc
  app/zelda3/dungeon/dungeon_data.h
  app/zelda3/dungeon/dungeon_loader.cc
  app/zelda3/dungeon/dungeon_loader.h
  app/zelda3/dungeon/dungeon_room.cc
  app/zelda3/dungeon/dungeon_room.h
  app/zelda3/dungeon/dungeon_sprite.cc
  app/zelda3/dungeon/dungeon_sprite.h
  app/zelda3/dungeon/dungeon_tile.cc
  app/zelda3/dungeon/dungeon_tile.h
  app/zelda3/dungeon/room_data.cc
  app/zelda3/dungeon/room_data.h
  app/zelda3/dungeon/room_properties.cc
  app/zelda3/dungeon/room_properties.h
  app/zelda3/dungeon/room_sprite.cc
  app/zelda3/dungeon/room_sprite.h
  app/zelda3/dungeon/room_tile.cc
  app/zelda3/dungeon/room_tile.h
)

# Zelda3 Music
source_group("Application\\Zelda3\\Music" FILES
  app/zelda3/music/music_data.cc
  app/zelda3/music/music_data.h
)

# Zelda3 Overworld
source_group("Application\\Zelda3\\Overworld" FILES
  app/zelda3/overworld/overworld_data.cc
  app/zelda3/overworld/overworld_data.h
  app/zelda3/overworld/overworld_loader.cc
  app/zelda3/overworld/overworld_loader.h
  app/zelda3/overworld/overworld_sprite.cc
  app/zelda3/overworld/overworld_sprite.h
  app/zelda3/overworld/overworld_tile.cc
  app/zelda3/overworld/overworld_tile.h
  app/zelda3/overworld/overworld_transport.cc
  app/zelda3/overworld/overworld_transport.h
)

# Zelda3 Screen
source_group("Application\\Zelda3\\Screen" FILES
  app/zelda3/screen/dungeon_map.cc
  app/zelda3/screen/dungeon_map.h
  app/zelda3/screen/inventory.cc
  app/zelda3/screen/inventory.h
  app/zelda3/screen/title_screen.cc
  app/zelda3/screen/title_screen.h
)

# Zelda3 Sprite
source_group("Application\\Zelda3\\Sprite" FILES
  app/zelda3/sprite/sprite_data.cc
  app/zelda3/sprite/sprite_data.h
  app/zelda3/sprite/sprite_loader.cc
  app/zelda3/sprite/sprite_loader.h
  app/zelda3/sprite/sprite.cc
  app/zelda3/sprite/sprite.h
)

# Testing
source_group("Application\\Testing" FILES
  app/test/test_manager.cc
  app/test/test_manager.h
  app/test/e2e_test_suite.h
  app/test/integrated_test_suite.h
  app/test/rom_dependent_test_suite.h
  app/test/unit_test_suite.h
  app/test/zscustomoverworld_test_suite.h
)

# CLI Tools
source_group("CLI" FILES
  cli/cli_main.cc
  cli/tui/tui.cc
  cli/tui/tui.h
  cli/tui/unified_layout.cc
  cli/tui/unified_layout.h
  cli/tui/enhanced_chat_component.cc
  cli/tui/enhanced_chat_component.h
  cli/tui/enhanced_status_panel.cc
  cli/tui/enhanced_status_panel.h
  cli/cli.cc
  cli/cli.h
)

source_group("CLI\\Handlers" FILES
  cli/handlers/compress.cc
  cli/handlers/patch.cc
  cli/handlers/tile16_transfer.cc
)

# Utilities
source_group("Utilities" FILES
  util/bps.cc
  util/bps.h
  util/flag.cc
  util/flag.h
  util/hex.cc
  util/hex.h
  util/log.cc
  util/log.h
  util/macro.h
  util/notify.h
)

# API
source_group("API" FILES
  api/service_handler.cc
  api/yaze.proto
)

source_group("API\\Python" FILES
  api/python/yaze_py.cc
)

# Platform-specific Resources
source_group("Platform\\Windows" FILES
  app/platform/win32/yaze.rc
)

source_group("Platform\\iOS" FILES
  ios/main.mm
  ios/iOS/Info-iOS.plist
  ios/iOS/LaunchScreen.storyboard
  ios/macOS/Info-macOS.plist
  ios/macOS/MainMenu.storyboard
)

source_group("Platform\\iOS\\Assets" FILES
  ios/Media.xcassets/Contents.json
)

# Configuration
source_group("Configuration" FILES
  yaze_config.h.in
)

# Assets
source_group("Assets\\Fonts" FILES
  ${CMAKE_SOURCE_DIR}/assets/font/Karla-Regular.ttf
  ${CMAKE_SOURCE_DIR}/assets/font/Roboto-Medium.ttf
  ${CMAKE_SOURCE_DIR}/assets/font/Cousine-Regular.ttf
  ${CMAKE_SOURCE_DIR}/assets/font/DroidSans.ttf
  ${CMAKE_SOURCE_DIR}/assets/font/NotoSansJP.ttf
  ${CMAKE_SOURCE_DIR}/assets/font/IBMPlexSansJP-Bold.ttf
  ${CMAKE_SOURCE_DIR}/assets/font/MaterialIcons-Regular.ttf
)

source_group("Assets\\Themes" FILES
  ${YAZE_THEME_FILES}
)

source_group("Assets\\Layouts" FILES
  ${CMAKE_SOURCE_DIR}/assets/layouts/ow_toolset.zeml
)

source_group("Assets\\Library" FILES
  ${CMAKE_SOURCE_DIR}/assets/lib/libasar.dll
)
