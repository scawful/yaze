# yaze source files -----------------------------------------------------------
set(
  YAZE_APP_CORE_SRC
  app/core/common.cc
  app/core/controller.cc
)

set(
  YAZE_APP_EDITOR_SRC
  app/editor/assembly_editor.cc
  app/editor/dungeon_editor.cc
  app/editor/master_editor.cc
  app/editor/music_editor.cc
  app/editor/overworld_editor.cc
  app/editor/palette_editor.cc
  app/editor/screen_editor.cc
)

set(
  YAZE_APP_GFX_SRC
  app/gfx/bitmap.cc
  app/gfx/snes_palette.cc
  app/gfx/snes_tile.cc
)

set(
  YAZE_APP_ZELDA3_SRC
  app/zelda3/inventory.cc
  app/zelda3/overworld_map.cc
  app/zelda3/overworld.cc
  app/zelda3/title_screen.cc
  app/zelda3/sprite.cc
)

set(
  YAZE_GUI_SRC
  gui/canvas.cc
  gui/input.cc
  gui/style.cc
  gui/widgets.cc
  gui/color.cc
)

# executable creation ---------------------------------------------------------

add_executable(
  yaze
  app/yaze.cc
  app/rom.cc
  ${YAZE_APP_CORE_SRC}
  ${YAZE_APP_EDITOR_SRC}
  ${YAZE_APP_GFX_SRC}
  ${YAZE_APP_ZELDA3_SRC}
  ${YAZE_GUI_SRC}
  ${ASAR_STATIC_SRC}
  ${IMGUI_SRC}
)

# including libraries ---------------------------------------------------------

target_include_directories(
  yaze PUBLIC
  lib/
  app/
  ${CMAKE_SOURCE_DIR}/src/
  ${PNG_INCLUDE_DIRS}
  ${SDL2_INCLUDE_DIR}
  ${GLEW_INCLUDE_DIRS}
  lib/asar/src/asar/
)

set(SDL_TARGETS SDL2::SDL2)

if(WIN32 OR MINGW)
    list(PREPEND SDL_TARGETS SDL2::SDL2main)
    add_definitions(-DSDL_MAIN_HANDLED)
endif()

# linking libraries -----------------------------------------------------------

target_link_libraries(
  yaze PUBLIC
  ${ABSL_TARGETS}
  ${SDL_TARGETS}
  ${PNG_LIBRARIES}
  ${GLEW_LIBRARIES}
  ${OPENGL_LIBRARIES}
  ${CMAKE_DL_LIBS}
  asar-static
  ImGui
)

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
      # MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/cmake/yaze.plist.in
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
    LINK_FLAGS "${CMAKE_CURRENT_SOURCE_DIR}/yaze.res"
)
endif()

# add_subdirectory(app/delta)

# add_executable(
#   yaze_delta
#   app/delta/delta.cc
#   app/delta/viewer.cc
#   app/delta/service.cc
#   app/delta/client.cc
#   app/rom.cc
#   ${YAZE_APP_ASM_SRC}
#   ${YAZE_APP_CORE_SRC}
#   ${YAZE_APP_EDITOR_SRC}
#   ${YAZE_APP_GFX_SRC}
#   ${YAZE_APP_ZELDA3_SRC}
#   ${YAZE_GUI_SRC}
#   ${IMGUI_SRC}
#   ${ASAR_STATIC_SRC}
# )

# target_include_directories(
#   yaze_delta PUBLIC
#   lib/
#   app/
#   lib/asar/src/
#   ${ASAR_INCLUDE_DIR}
#   ${CMAKE_SOURCE_DIR}/src/
#   ${PNG_INCLUDE_DIRS}
#   ${SDL2_INCLUDE_DIR}
#   ${GLEW_INCLUDE_DIRS}
# )

# target_link_libraries(
#   yaze_delta PUBLIC
#   ${ABSL_TARGETS}
#   ${SDL_TARGETS}
#   ${PNG_LIBRARIES}
#   ${GLEW_LIBRARIES}
#   ${OPENGL_LIBRARIES}
#   ${CMAKE_DL_LIBS}
#   delta-service
#   asar-static
#   ImGui
# )
# target_compile_definitions(yaze_delta PRIVATE "linux")
# target_compile_definitions(yaze_delta PRIVATE "stricmp=strcasecmp")

# set (source "${CMAKE_SOURCE_DIR}/assets")
# set (destination "${CMAKE_CURRENT_BINARY_DIR}/assets")
# add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
#                    COMMAND ${CMAKE_COMMAND} -E create_symlink ${source} ${destination} 
#                    DEPENDS ${destination}
#                    COMMENT "symbolic link resources folder from ${source} => ${destination}")