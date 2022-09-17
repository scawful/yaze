# gui libraries ---------------------------------------------------------------
set(IMGUI_PATH  "lib/imgui")
file(GLOB IMGUI_SOURCES ${IMGUI_PATH}/*.cpp)
add_library("ImGui" STATIC ${IMGUI_SOURCES})
target_include_directories("ImGui" PUBLIC ${IMGUI_PATH})
target_include_directories(ImGui PUBLIC ${SDL2_INCLUDE_DIR})
target_compile_definitions(ImGui PUBLIC 
  IMGUI_IMPL_OPENGL_LOADER_CUSTOM=<SDL2/SDL_opengl.h>  GL_GLEXT_PROTOTYPES=1)

set(IMGUI_FILE_DLG_PATH "lib/ImGuiFileDialog")
file(GLOB IMGUI_FILE_DLG_SOURCES ${IMGUI_FILE_DLG_PATH}/*.cpp)
add_library("ImGuiFileDialog" STATIC ${IMGUI_FILE_DLG_SOURCES})
target_include_directories(ImGuiFileDialog PUBLIC ${IMGUI_PATH})
target_compile_definitions(ImGuiFileDialog PUBLIC 
  IMGUI_IMPL_OPENGL_LOADER_CUSTOM=<SDL2/SDL_opengl.h>  GL_GLEXT_PROTOTYPES=1)

set(IMGUI_COLOR_TEXT_EDIT_PATH "lib/ImGuiColorTextEdit")
file(GLOB IMGUI_COLOR_TEXT_EDIT_SOURCES ${IMGUI_COLOR_TEXT_EDIT_PATH}/*.cpp)
add_library("ImGuiColorTextEdit" STATIC ${IMGUI_COLOR_TEXT_EDIT_SOURCES})
target_include_directories(ImGuiColorTextEdit PUBLIC ${IMGUI_PATH})
target_compile_definitions(ImGuiColorTextEdit PUBLIC 
  IMGUI_IMPL_OPENGL_LOADER_CUSTOM=<SDL2/SDL_opengl.h>  GL_GLEXT_PROTOTYPES=1)

set(
  IMGUI_SRC
  ${IMGUI_PATH}/imgui.cpp 
  ${IMGUI_PATH}/imgui_demo.cpp
  ${IMGUI_PATH}/imgui_draw.cpp 
  ${IMGUI_PATH}/imgui_widgets.cpp
  ${IMGUI_PATH}/backends/imgui_impl_sdl.cpp
  ${IMGUI_PATH}/backends/imgui_impl_sdlrenderer.cpp 
  ${IMGUI_PATH}/misc/cpp/imgui_stdlib.cpp
  ${IMGUI_FILE_DLG_PATH}/ImGuiFileDialog.cpp
  ${IMGUI_COLOR_TEXT_EDIT_PATH}/TextEditor.cpp
)

# Asar Assembly ---------------------------------------------------------------
add_subdirectory(lib/asar/src)
get_target_property(ASAR_INCLUDE_DIR asar-static INCLUDE_DIRECTORIES)
target_include_directories(asar-static PRIVATE ${ASAR_INCLUDE_DIR})
set(ASAR_GEN_EXE OFF)
set(ASAR_GEN_DLL ON)
set(ASAR_GEN_LIB ON)
set(ASAR_GEN_EXE_TEST OFF)
set(ASAR_GEN_DLL_TEST OFF)

set(ASAR_STATIC_SRC
  "lib/asar/src/asar/interface-lib.cpp"
  "lib/asar/src/asar/addr2line.cpp"
  "lib/asar/src/asar/arch-65816.cpp"
  "lib/asar/src/asar/arch-spc700.cpp"
  "lib/asar/src/asar/arch-superfx.cpp"
  "lib/asar/src/asar/assembleblock.cpp"
  "lib/asar/src/asar/crc32.cpp"
  "lib/asar/src/asar/libcon.cpp"
  "lib/asar/src/asar/libsmw.cpp"
  "lib/asar/src/asar/libstr.cpp"
  "lib/asar/src/asar/macro.cpp"
  "lib/asar/src/asar/main.cpp"
  "lib/asar/src/asar/asar_math.cpp"
  "lib/asar/src/asar/virtualfile.cpp"
  "lib/asar/src/asar/warnings.cpp"
  "lib/asar/src/asar/errors.cpp"
  "lib/asar/src/asar/platform/file-helpers.cpp"
  "lib/asar/src/asar/platform/linux/file-helpers-linux.cpp"
)

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
  YAZE_APP_ASM_SRC
  app/asm/script.cc
)

set(
  YAZE_GUI_SRC
  gui/canvas.cc
  gui/input.cc
  gui/style.cc
  gui/widgets.cc
  gui/color.cc
)

add_executable(
  yaze
  app/yaze.cc
  app/rom.cc
  ${YAZE_APP_ASM_SRC}
  ${YAZE_APP_CORE_SRC}
  ${YAZE_APP_EDITOR_SRC}
  ${YAZE_APP_GFX_SRC}
  ${YAZE_APP_ZELDA3_SRC}
  ${YAZE_GUI_SRC}
  ${IMGUI_SRC}
  ${ASAR_STATIC_SRC}
)

target_include_directories(
  yaze PUBLIC
  lib/
  app/
  ${ASAR_INCLUDE_DIR}
  ${CMAKE_SOURCE_DIR}/src/
  ${PNG_INCLUDE_DIRS}
  ${SDL2_INCLUDE_DIR}
  ${GLEW_INCLUDE_DIRS}
  lib/asar/src/
)

set(SDL_TARGETS SDL2::SDL2)

if(WIN32 OR MINGW)
    list(PREPEND SDL_TARGETS SDL2::SDL2main)
    add_definitions(-DSDL_MAIN_HANDLED)
endif()

target_link_libraries(
  yaze PUBLIC
  ${ABSL_TARGETS}
  ${SDL_TARGETS}
  ${PNG_LIBRARIES}
  ${GLEW_LIBRARIES}
  ${OPENGL_LIBRARIES}
  ${CMAKE_DL_LIBS}
  ImGui
  asar-static
)
target_compile_definitions(yaze PRIVATE "linux")
target_compile_definitions(yaze PRIVATE "stricmp=strcasecmp")

set_target_properties(yaze
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LINK_FLAGS "${CMAKE_CURRENT_SOURCE_DIR}/yaze.res"
)

add_subdirectory(app/delta)

add_executable(
  yaze_delta
  app/delta/delta.cc
  app/delta/viewer.cc
  app/delta/client.cc
  app/rom.cc
  ${YAZE_APP_ASM_SRC}
  ${YAZE_APP_CORE_SRC}
  ${YAZE_APP_EDITOR_SRC}
  ${YAZE_APP_GFX_SRC}
  ${YAZE_APP_ZELDA3_SRC}
  ${YAZE_GUI_SRC}
  ${IMGUI_SRC}
)

target_include_directories(
  yaze_delta PUBLIC
  lib/
  app/
  ${ASAR_INCLUDE_DIR}
  ${CMAKE_SOURCE_DIR}/src/
  ${PNG_INCLUDE_DIRS}
  ${SDL2_INCLUDE_DIR}
  ${GLEW_INCLUDE_DIRS}
  ${ASAR_STATIC_SRC}
)

target_link_libraries(
  yaze_delta PUBLIC
  ${ABSL_TARGETS}
  ${SDL_TARGETS}
  ${PNG_LIBRARIES}
  ${GLEW_LIBRARIES}
  ${OPENGL_LIBRARIES}
  ${CMAKE_DL_LIBS}
  delta-service
  asar-static
  ImGui
)

set (source "${CMAKE_SOURCE_DIR}/assets")
set (destination "${CMAKE_CURRENT_BINARY_DIR}/assets")
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E create_symlink ${source} ${destination} 
                   DEPENDS ${destination}
                   COMMENT "symbolic link resources folder from ${source} => ${destination}")