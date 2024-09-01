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

if (YAZE_BUILD_APP)
  include(app/app.cmake)
endif()
if (YAZE_BUILD_EMU)
  include(app/emu/emu.cmake)
endif()
if (YAZE_BUILD_Z3ED)
  include(cli/z3ed.cmake)
endif()
if (YAZE_BUILD_PYTHON)
  include(py/yaze_py.cmake)
endif()
if (YAZE_BUILD_TESTS)
  include(test/CMakeLists.txt)
endif()

if (UNIX)
  target_compile_definitions(yaze PRIVATE "linux")
  target_compile_definitions(yaze PRIVATE "stricmp=strcasecmp")
endif()

if(MACOS)
  set(MACOSX_BUNDLE_ICON_FILE ${CMAKE_SOURCE_DIR}/yaze.ico)
  set_target_properties(yaze
      PROPERTIES
      BUNDLE True
      ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
      LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
      RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
      MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/cmake/yaze.plist.in
  )
elseif(UNIX)
  set_target_properties(yaze
    PROPERTIES
    BUNDLE True
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  )
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
    ${IMGUI_SRC}
    ${IMGUI_TEST_ENGINE_SOURCES}
  )

  target_include_directories(
    yaze_c PUBLIC
    lib/
    app/
    ${CMAKE_SOURCE_DIR}/src/
    ${PNG_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIR}
  )

  target_link_libraries(
    yaze_c PRIVATE
    ${ABSL_TARGETS}
    ${SDL_TARGETS}
    ${PNG_LIBRARIES}
    ${CMAKE_DL_LIBS}
    ImGuiTestEngine
    ImGui
  )

  if (YAZE_INSTALL_LIB)
    install(TARGETS yaze_c
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib/static)

    install(
    FILES
      yaze.h
      incl/sprite.h
      incl/snes_color.h
      incl/overworld.h
    DESTINATION include)
  endif()
endif()
