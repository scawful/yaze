set(
  YAZE_APP_EMU_SRC
  app/emu/audio/apu.cc
  app/emu/audio/spc700.cc
  app/emu/audio/dsp.cc
  app/emu/audio/internal/addressing.cc
  app/emu/audio/internal/instructions.cc
  app/emu/cpu/internal/addressing.cc
  app/emu/cpu/internal/instructions.cc
  app/emu/cpu/cpu.cc
  app/emu/video/ppu.cc
  app/emu/memory/dma.cc
  app/emu/memory/memory.cc
  app/emu/snes.cc
)

set(
  YAZE_UTIL_SRC
  util/bps.cc
  util/flag.cc
  util/hex.cc
)

set(YAZE_RESOURCE_FILES
  ${CMAKE_SOURCE_DIR}/assets/layouts/overworld.zeml
  ${CMAKE_SOURCE_DIR}/assets/font/Karla-Regular.ttf
  ${CMAKE_SOURCE_DIR}/assets/font/Roboto-Medium.ttf
  ${CMAKE_SOURCE_DIR}/assets/font/Cousine-Regular.ttf
  ${CMAKE_SOURCE_DIR}/assets/font/DroidSans.ttf
  ${CMAKE_SOURCE_DIR}/assets/font/NotoSansJP.ttf
  ${CMAKE_SOURCE_DIR}/assets/font/IBMPlexSansJP-Bold.ttf
  ${CMAKE_SOURCE_DIR}/assets/font/MaterialIcons-Regular.ttf
)

foreach (FILE ${YAZE_RESOURCE_FILES})
  file(RELATIVE_PATH NEW_FILE "${CMAKE_SOURCE_DIR}/assets" ${FILE})
  get_filename_component(NEW_FILE_PATH ${NEW_FILE} DIRECTORY)
  set_source_files_properties(${FILE}
    PROPERTIES
    MACOSX_PACKAGE_LOCATION "Resources/${NEW_FILE_PATH}"
  )
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

if (YAZE_BUILD_APP)
  include(app/app.cmake)
endif()
if (YAZE_BUILD_EMU)
  include(app/emu/emu.cmake)
endif()
if (YAZE_BUILD_Z3ED)
  include(cli/z3ed.cmake)
endif()

if(MACOS)
  set(MACOSX_BUNDLE_ICON_FILE ${CMAKE_SOURCE_DIR}/win32/yaze.ico)
  set(MACOSX_BUNDLE_BUNDLE_NAME "yaze")
  set(MACOSX_BUNDLE_EXECUTABLE_NAME "yaze")
  set(MACOSX_BUNDLE_GUI_IDENTIFIER "yaze")
  set(MACOSX_BUNDLE_INFO_STRING "yaze")
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
  set_target_properties(yaze
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LINK_FLAGS "${CMAKE_CURRENT_SOURCE_DIR}/win32/yaze.res"
  )
endif()

# Yaze C API
if (YAZE_BUILD_LIB)
  add_library(
    yaze_c SHARED
    ./yaze.cc
    app/rom.cc
    ${YAZE_APP_EMU_SRC}
    ${YAZE_APP_CORE_SRC}
    ${YAZE_APP_EDITOR_SRC}
    ${YAZE_APP_GFX_SRC}
    ${YAZE_APP_ZELDA3_SRC}
    ${YAZE_GUI_SRC}
    ${YAZE_UTIL_SRC}
    ${IMGUI_SRC}
    ${IMGUI_TEST_ENGINE_SOURCES}
  )

  target_include_directories(
    yaze_c PUBLIC
    lib/
    app/
    ${CMAKE_SOURCE_DIR}/incl/
    ${CMAKE_SOURCE_DIR}/src/
    ${ASAR_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIR}
    ${PROJECT_BINARY_DIR}
  )
  
  # Conditionally add PNG include dirs if available
  if(PNG_FOUND)
    target_include_directories(yaze_c PUBLIC ${PNG_INCLUDE_DIRS})
  endif()

  target_link_libraries(
    yaze_c PRIVATE
    asar-static
    ${ABSL_TARGETS}
    ${SDL_TARGETS}
    ${CMAKE_DL_LIBS}
    ImGui
  )
  
  # Conditionally link ImGui Test Engine
  if(YAZE_ENABLE_UI_TESTS AND TARGET ImGuiTestEngine)
    target_link_libraries(yaze_c PRIVATE ImGuiTestEngine)
  endif()
  
  # Conditionally link PNG if available
  if(PNG_FOUND)
    target_link_libraries(yaze_c PRIVATE ${PNG_LIBRARIES})
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
