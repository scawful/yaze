# Dear ImGui dependency management
# Uses the bundled ImGui in ext/imgui when available, otherwise falls back to FetchContent.

message(NOTICE "Configuring Dear ImGui support")

set(_yaze_imgui_version "1.91.2")
set(_yaze_imgui_ext_dir "${CMAKE_SOURCE_DIR}/ext/imgui")
set(IMGUI_DIR ${_yaze_imgui_ext_dir})

if(EXISTS "${_yaze_imgui_ext_dir}/imgui.cpp")
  message(NOTICE "Setting up Dear ImGui from ext/imgui bundle")
else()
  message(NOTICE "ext/imgui missing; fetching Dear ImGui ${_yaze_imgui_version} via FetchContent")
  include(FetchContent)
  FetchContent_Declare(
    imgui_external
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v${_yaze_imgui_version}
    GIT_SHALLOW    TRUE
  )
  FetchContent_MakeAvailable(imgui_external)
  set(IMGUI_DIR ${imgui_external_SOURCE_DIR})
endif()

# Create ImGui library with core files from the resolved source directory
add_library(ImGui STATIC
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_demo.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
  # SDL2 backend
  ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
  ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer2.cpp
  # C++ stdlib helpers (for std::string support)
  ${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp
)

target_include_directories(ImGui PUBLIC
  ${IMGUI_DIR}
  ${IMGUI_DIR}/backends
)

# Set C++ standard requirement (ImGui 1.90+ requires C++11, we use C++17 for consistency)
target_compile_features(ImGui PUBLIC cxx_std_17)

# Link to SDL2
target_link_libraries(ImGui PUBLIC ${YAZE_SDL2_TARGETS})

message(STATUS "Created ImGui target from source at ${IMGUI_DIR}")

# Create ImGui Test Engine for test automation (if tests are enabled and source exists)
if(YAZE_BUILD_TESTS)
  set(IMGUI_TEST_ENGINE_DIR ${CMAKE_SOURCE_DIR}/ext/imgui_test_engine/imgui_test_engine)
  
  if(EXISTS ${IMGUI_TEST_ENGINE_DIR})
    set(IMGUI_TEST_ENGINE_SOURCES
      ${IMGUI_TEST_ENGINE_DIR}/imgui_te_context.cpp
      ${IMGUI_TEST_ENGINE_DIR}/imgui_te_coroutine.cpp
      ${IMGUI_TEST_ENGINE_DIR}/imgui_te_engine.cpp
      ${IMGUI_TEST_ENGINE_DIR}/imgui_te_exporters.cpp
      ${IMGUI_TEST_ENGINE_DIR}/imgui_te_perftool.cpp
      ${IMGUI_TEST_ENGINE_DIR}/imgui_te_ui.cpp
      ${IMGUI_TEST_ENGINE_DIR}/imgui_te_utils.cpp
      ${IMGUI_TEST_ENGINE_DIR}/imgui_capture_tool.cpp
    )
    
    add_library(ImGuiTestEngine STATIC ${IMGUI_TEST_ENGINE_SOURCES})
    target_include_directories(ImGuiTestEngine PUBLIC 
      ${IMGUI_DIR}
      ${IMGUI_TEST_ENGINE_DIR}
      ${CMAKE_SOURCE_DIR}/ext
    )
    target_compile_features(ImGuiTestEngine PUBLIC cxx_std_17)
    target_link_libraries(ImGuiTestEngine PUBLIC ImGui ${YAZE_SDL2_TARGETS})
    target_compile_definitions(ImGuiTestEngine PUBLIC
      IMGUI_ENABLE_TEST_ENGINE=1
      IMGUI_TEST_ENGINE_ENABLE_COROUTINE_STDTHREAD_IMPL=1
    )
    
    message(STATUS "Created ImGuiTestEngine target for test automation")
  endif()
endif()

# Export ImGui targets for use in other CMake files  
set(YAZE_IMGUI_TARGETS ImGui)

message(STATUS "Dear ImGui setup complete - YAZE_IMGUI_TARGETS = ${YAZE_IMGUI_TARGETS}")
