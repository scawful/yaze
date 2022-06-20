# find libraries --------------------------------------------------------------------------------------------------
find_package(SDL2 REQUIRED)
find_package(PNG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)

# gui libraries ---------------------------------------------------------------------------------------------------
set(IMGUI_PATH  "Library/imgui")
file(GLOB IMGUI_SOURCES ${IMGUI_PATH}/*.cpp)
add_library("ImGui" STATIC ${IMGUI_SOURCES})
target_include_directories("ImGui" PUBLIC ${IMGUI_PATH})
target_include_directories(ImGui PUBLIC ${SDL2_INCLUDE_DIR})
target_compile_definitions(ImGui PUBLIC IMGUI_IMPL_OPENGL_LOADER_CUSTOM=<SDL2/SDL_opengl.h>  GL_GLEXT_PROTOTYPES=1)

set(IMGUI_FILE_DLG_PATH "Library/ImGuiFileDialog")
file(GLOB IMGUI_FILE_DLG_SOURCES ${IMGUI_FILE_DLG_PATH}/*.cpp)
add_library("ImGuiFileDialog" STATIC ${IMGUI_FILE_DLG_SOURCES})
target_include_directories(ImGuiFileDialog PUBLIC ${IMGUI_PATH})
target_compile_definitions(ImGuiFileDialog PUBLIC IMGUI_IMPL_OPENGL_LOADER_CUSTOM=<SDL2/SDL_opengl.h>  GL_GLEXT_PROTOTYPES=1)

set(IMGUI_COLOR_TEXT_EDIT_PATH "Library/ImGuiColorTextEdit")
file(GLOB IMGUI_COLOR_TEXT_EDIT_SOURCES ${IMGUI_COLOR_TEXT_EDIT_PATH}/*.cpp)
add_library("ImGuiColorTextEdit" STATIC ${IMGUI_COLOR_TEXT_EDIT_SOURCES})
target_include_directories(ImGuiColorTextEdit PUBLIC ${IMGUI_PATH})
target_compile_definitions(ImGuiColorTextEdit PUBLIC IMGUI_IMPL_OPENGL_LOADER_CUSTOM=<SDL2/SDL_opengl.h>  GL_GLEXT_PROTOTYPES=1)

set(SNESHACKING_PATH "Library/sneshacking/src")
file(GLOB SNESHACKING_SOURCES ${SNESHACKING_PATH}/*.c)
add_library("NintendoCompression" STATIC ${SNESHACKING_SOURCES})

# Executable Linkage --------------------------------------------------------------------------------------
add_executable(
    yaze
    yaze.cc
    gui/input.cc
    gui/style.cc
    gui/editor/editor.cc
    gui/editor/overworld_editor.cc
    app/rom.cc
    app/core/constants.cc
    app/core/controller.cc
    app/gfx/bitmap.cc
    app/gfx/tile.cc
    app/gfx/snes_palette.cc
    app/zelda3/overworld.cc
    app/zelda3/overworld_map.cc
    # GUI libraries 
    ${IMGUI_PATH}/imgui.cpp 
    ${IMGUI_PATH}/imgui_demo.cpp
    ${IMGUI_PATH}/imgui_draw.cpp 
    ${IMGUI_PATH}/imgui_widgets.cpp
    ${IMGUI_PATH}/backends/imgui_impl_sdl.cpp
    ${IMGUI_PATH}/backends/imgui_impl_sdlrenderer.cpp 
    ${IMGUI_PATH}/misc/cpp/imgui_stdlib.cpp
    ${IMGUI_FILE_DLG_PATH}/ImGuiFileDialog.cpp
    ${IMGUI_COLOR_TEXT_EDIT_PATH}/TextEditor.cpp
    ${SNESHACKING_PATH}/compressions/alttpcompression.c
    ${SNESHACKING_PATH}/compressions/stdnintendo.c
    ${SNESHACKING_PATH}/tile.c
    ${SNESHACKING_PATH}/tilepng.c
    ${SNESHACKING_PATH}/palette.c
    ${SNESHACKING_PATH}/rommapping.c
    ${SNESHACKING_PATH}/mapping_lorom.c
)

target_include_directories(
  yaze PUBLIC
  /
  Library/
  app/
  "C:/msys64/mingw64/include/libpng16"
  "C:/msys64/mingw64/include/SDL2"
  "C:/msys64/mingw64/include"
  ${SNESHACKING_PATH}
  ${PNG_INCLUDE_DIRS}
  ${SDL2_INCLUDE_DIR}
  ${GLEW_INCLUDE_DIRS}
)

target_link_libraries(
  yaze
  libstdc++ 
  SDL2main
  ${PNG_LIBRARIES}
  ${SDL2_LIBRARIES} 
  ${GLEW_LIBRARIES}
  ${OPENGL_LIBRARIES} 
  ImGui
  NintendoCompression
)

set_target_properties(yaze
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set (source "${CMAKE_SOURCE_DIR}/assets")
set (destination "${CMAKE_CURRENT_BINARY_DIR}/assets")
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E create_symlink ${source} ${destination} 
                   DEPENDS ${destination}
                   COMMENT "symbolic link resources folder from ${source} => ${destination}")