# gui libraries ---------------------------------------------------------------
set(IMGUI_PATH  ${CMAKE_SOURCE_DIR}/src/lib/imgui)
file(GLOB IMGUI_SOURCES ${IMGUI_PATH}/*.cpp)
add_library("ImGui" STATIC ${IMGUI_SOURCES})
target_include_directories("ImGui" PUBLIC ${IMGUI_PATH})
target_include_directories(ImGui PUBLIC ${SDL2_INCLUDE_DIR})
target_compile_definitions(ImGui PUBLIC
  IMGUI_IMPL_OPENGL_LOADER_CUSTOM=<SDL2/SDL_opengl.h>  GL_GLEXT_PROTOTYPES=1)

set(IMGUI_FILE_DLG_PATH ${CMAKE_SOURCE_DIR}/src/lib/ImGuiFileDialog)
file(GLOB IMGUI_FILE_DLG_SOURCES ${IMGUI_FILE_DLG_PATH}/*.cpp)
add_library("ImGuiFileDialog" STATIC ${IMGUI_FILE_DLG_SOURCES})
target_include_directories(ImGuiFileDialog PUBLIC ${IMGUI_PATH}  ${CMAKE_SOURCE_DIR}/src/lib)

set(IMGUI_COLOR_TEXT_EDIT_PATH ${CMAKE_SOURCE_DIR}/src/lib/ImGuiColorTextEdit)
file(GLOB IMGUI_COLOR_TEXT_EDIT_SOURCES ${IMGUI_COLOR_TEXT_EDIT_PATH}/*.cpp)
add_library("ImGuiColorTextEdit" STATIC ${IMGUI_COLOR_TEXT_EDIT_SOURCES})
target_include_directories(ImGuiColorTextEdit PUBLIC ${IMGUI_PATH} ${CMAKE_SOURCE_DIR}/src/lib)

set(IMGUI_TEST_ENGINE_PATH ${CMAKE_SOURCE_DIR}/src/lib/imgui_test_engine/imgui_test_engine)
file(GLOB IMGUI_TEST_ENGINE_SOURCES ${IMGUI_TEST_ENGINE_PATH}/*.cpp)
add_library("ImGuiTestEngine" STATIC ${IMGUI_TEST_ENGINE_SOURCES})
target_include_directories(ImGuiTestEngine PUBLIC ${IMGUI_PATH} ${CMAKE_SOURCE_DIR}/src/lib)
target_link_libraries(ImGuiTestEngine PUBLIC ImGui)

set(
  IMGUI_SRC
  ${IMGUI_PATH}/imgui.cpp
  ${IMGUI_PATH}/imgui_demo.cpp
  ${IMGUI_PATH}/imgui_draw.cpp
  ${IMGUI_PATH}/imgui_widgets.cpp
  ${IMGUI_PATH}/backends/imgui_impl_sdl2.cpp
  ${IMGUI_PATH}/backends/imgui_impl_sdlrenderer2.cpp
  ${IMGUI_PATH}/misc/cpp/imgui_stdlib.cpp
  ${IMGUI_FILE_DLG_PATH}/ImGuiFileDialog.cpp
  ${IMGUI_COLOR_TEXT_EDIT_PATH}/TextEditor.cpp
)

# For integration test
add_definitions("-DIMGUI_ENABLE_TEST_ENGINE -DIMGUI_TEST_ENGINE_ENABLE_COROUTINE_STDTHREAD_IMPL=1")
