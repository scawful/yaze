# SDL libraries --------------------------------------------------------------------------------------------------
find_package(SDL2 REQUIRED)

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)

# ImGui library --------------------------------------------------------------------------------------------------
set(IMGUI_PATH  "Library/imgui")                         # Set where the ImGui files are stored
file(GLOB IMGUI_SOURCES ${IMGUI_PATH}/*.cpp)             # Compile as static library 
add_library("ImGui" STATIC ${IMGUI_SOURCES})
target_include_directories("ImGui" PUBLIC ${IMGUI_PATH})
target_include_directories(ImGui PUBLIC ${SDL2_INCLUDE_DIR})
target_compile_definitions(ImGui PUBLIC IMGUI_IMPL_OPENGL_LOADER_CUSTOM=<SDL2/SDL_opengl.h>  GL_GLEXT_PROTOTYPES=1)

# ImGuiFileDialog
set(IMGUI_FILE_DLG_PATH "Library/ImGuiFileDialog")
file(GLOB IMGUI_FILE_DLG_SOURCES ${IMGUI_FILE_DLG_PATH}/*.cpp)
add_library("ImGuiFileDialog" STATIC ${IMGUI_FILE_DLG_SOURCES})
target_include_directories(ImGuiFileDialog PUBLIC ${IMGUI_PATH})
target_include_directories(ImGuiFileDialog PUBLIC ${SDL2_INCLUDE_DIR})
target_compile_definitions(ImGuiFileDialog PUBLIC IMGUI_IMPL_OPENGL_LOADER_CUSTOM=<SDL2/SDL_opengl.h>  GL_GLEXT_PROTOTYPES=1)

# Executable Linkage --------------------------------------------------------------------------------------
add_executable(
    yaze
    yaze.cc
    Application/Core/Controller.cc
    Application/Core/Renderer.cc
    Application/Core/Window.cc
    Application/Data/Overworld.cc
    Application/Data/OverworldMap.cc
    Application/Graphics/Bitmap.cc
    Application/Graphics/Tile.cc
    Application/Editor/Editor.cc
    Application/Editor/OverworldEditor.cc
    Application/Events/Event.cc
    Application/Utils/Compression.cc
    Application/Utils/ROM.cc
    # GUI libraries 
    ${IMGUI_PATH}/imgui.cpp 
    ${IMGUI_PATH}/imgui_demo.cpp
    ${IMGUI_PATH}/imgui_draw.cpp 
    ${IMGUI_PATH}/imgui_widgets.cpp
    ${IMGUI_PATH}/backends/imgui_impl_sdl.cpp
    ${IMGUI_PATH}/backends/imgui_impl_sdlrenderer.cpp 
    ${IMGUI_PATH}/misc/cpp/imgui_stdlib.cpp
    ${IMGUI_FILE_DLG_PATH}/ImGuiFileDialog.cpp
)

target_include_directories(
  yaze PUBLIC
  Library/
  Application/
  Resources/
  ${SDL2_INCLUDE_DIR}
  ${GLEW_INCLUDE_DIRS}
)

target_link_libraries(
  yaze 
  ${SDL2_LIBRARIES} 
  ${GLEW_LIBRARIES}
  ${OPENGL_LIBRARIES} 
  png
  ImGui
)

set (source "${CMAKE_SOURCE_DIR}/assets")
set (destination "${CMAKE_CURRENT_BINARY_DIR}/assets")
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E create_symlink ${source} ${destination} 
                   DEPENDS ${destination}
                   COMMENT "symbolic link resources folder from ${source} => ${destination}")