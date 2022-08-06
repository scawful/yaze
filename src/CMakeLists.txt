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
  app/gfx/pseudo_vram.cc
  app/gfx/snes_palette.cc
  app/gfx/snes_tile.cc
)

set(
  YAZE_APP_ZELDA3_SRC
  app/zelda3/overworld_map.cc
  app/zelda3/overworld.cc
  app/zelda3/screen.cc
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
)

add_executable(
  yaze
  yaze.cc
  app/rom.cc
  ${YAZE_APP_ASM_SRC}
  ${YAZE_APP_CORE_SRC}
  ${YAZE_APP_EDITOR_SRC}
  ${YAZE_APP_GFX_SRC}
  ${YAZE_APP_ZELDA3_SRC}
  ${YAZE_GUI_SRC}
  ${IMGUI_SRC}
  lib/asar/src/asar-dll-bindings/c/asardll.c
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
  lib/asar/src/asar-dll-bindings/c
)

set(
  ABSL_TARGETS   
  absl::strings
  absl::flags
  absl::status
  absl::statusor
  absl::examine_stack
  absl::stacktrace
  absl::base
  absl::config
  absl::core_headers
  absl::raw_logging_internal
  absl::failure_signal_handler
  absl::flat_hash_map
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
  ImGui
)

set_target_properties(yaze
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LINK_FLAGS "${CMAKE_CURRENT_SOURCE_DIR}/yaze.res"
)

set (source "${CMAKE_SOURCE_DIR}/assets")
set (destination "${CMAKE_CURRENT_BINARY_DIR}/assets")
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E create_symlink ${source} ${destination} 
                   DEPENDS ${destination}
                   COMMENT "symbolic link resources folder from ${source} => ${destination}")