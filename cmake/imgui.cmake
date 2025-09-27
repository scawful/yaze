# gui libraries ---------------------------------------------------------------
set(IMGUI_PATH  ${CMAKE_SOURCE_DIR}/src/lib/imgui)
file(GLOB IMGUI_SOURCES ${IMGUI_PATH}/*.cpp)
set(IMGUI_BACKEND_SOURCES
  ${IMGUI_PATH}/backends/imgui_impl_sdl2.cpp
  ${IMGUI_PATH}/backends/imgui_impl_sdlrenderer2.cpp
  ${IMGUI_PATH}/misc/cpp/imgui_stdlib.cpp
)
add_library("ImGui" STATIC ${IMGUI_SOURCES} ${IMGUI_BACKEND_SOURCES})
target_include_directories("ImGui" PUBLIC ${IMGUI_PATH} ${IMGUI_PATH}/backends)
target_include_directories(ImGui PUBLIC ${SDL2_INCLUDE_DIR})
target_compile_definitions(ImGui PUBLIC
  IMGUI_IMPL_OPENGL_LOADER_CUSTOM=<SDL2/SDL_opengl.h>  GL_GLEXT_PROTOTYPES=1)

# Set up ImGui Test Engine sources and target conditionally
if(YAZE_ENABLE_UI_TESTS)
  set(IMGUI_TEST_ENGINE_PATH ${CMAKE_SOURCE_DIR}/src/lib/imgui_test_engine/imgui_test_engine)
  file(GLOB IMGUI_TEST_ENGINE_SOURCES ${IMGUI_TEST_ENGINE_PATH}/*.cpp)
  add_library("ImGuiTestEngine" STATIC ${IMGUI_TEST_ENGINE_SOURCES})
  target_include_directories(ImGuiTestEngine PUBLIC ${IMGUI_PATH} ${CMAKE_SOURCE_DIR}/src/lib)
  target_link_libraries(ImGuiTestEngine PUBLIC ImGui)
  
  # Enable test engine definitions only when UI tests are enabled
  target_compile_definitions(ImGuiTestEngine PUBLIC 
    IMGUI_ENABLE_TEST_ENGINE=1 
    IMGUI_TEST_ENGINE_ENABLE_COROUTINE_STDTHREAD_IMPL=1)
  
  # Also define for targets that link to ImGuiTestEngine
  set(IMGUI_TEST_ENGINE_DEFINITIONS 
    IMGUI_ENABLE_TEST_ENGINE=1 
    IMGUI_TEST_ENGINE_ENABLE_COROUTINE_STDTHREAD_IMPL=1)
  
  # Make ImGuiTestEngine target available
  set(IMGUI_TEST_ENGINE_TARGET ImGuiTestEngine)
else()
  # Create empty variables when UI tests are disabled
  set(IMGUI_TEST_ENGINE_SOURCES "")
  set(IMGUI_TEST_ENGINE_TARGET "")
  set(IMGUI_TEST_ENGINE_DEFINITIONS "")
endif()

set(
  IMGUI_SRC
  ${IMGUI_PATH}/imgui.cpp
  ${IMGUI_PATH}/imgui_demo.cpp
  ${IMGUI_PATH}/imgui_draw.cpp
  ${IMGUI_PATH}/imgui_widgets.cpp
  ${IMGUI_PATH}/backends/imgui_impl_sdl2.cpp
  ${IMGUI_PATH}/backends/imgui_impl_sdlrenderer2.cpp
  ${IMGUI_PATH}/misc/cpp/imgui_stdlib.cpp
)

