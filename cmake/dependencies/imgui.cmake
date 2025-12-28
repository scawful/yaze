# Dear ImGui dependency management
# Uses the bundled ImGui in ext/imgui

message(STATUS "Setting up Dear ImGui from bundled sources")

# Use the bundled ImGui from ext/imgui
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/ext/imgui)

# Select ImGui backend sources based on SDL version
if(YAZE_USE_SDL3)
  set(IMGUI_BACKEND_SOURCES
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer3.cpp
  )
  message(STATUS "Using ImGui SDL3 backend")
else()
  set(IMGUI_BACKEND_SOURCES
    ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer2.cpp
  )
  message(STATUS "Using ImGui SDL2 backend")
endif()

if(YAZE_PLATFORM_IOS)
  list(APPEND IMGUI_BACKEND_SOURCES
    ${IMGUI_DIR}/backends/imgui_impl_metal.mm
  )
endif()

# Create ImGui library with core files from bundled source
add_library(ImGui STATIC
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_demo.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
  # SDL backend (version-dependent)
  ${IMGUI_BACKEND_SOURCES}
  # C++ stdlib helpers (for std::string support)
  ${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp
)

target_include_directories(ImGui PUBLIC
  ${IMGUI_DIR}
  ${IMGUI_DIR}/backends
)

# Set C++ standard requirement (ImGui 1.90+ requires C++11, we use C++17 for consistency)
target_compile_features(ImGui PUBLIC cxx_std_17)

# Link to SDL (version-dependent)
if(YAZE_USE_SDL3)
  target_link_libraries(ImGui PUBLIC ${YAZE_SDL3_TARGETS})
else()
  target_link_libraries(ImGui PUBLIC ${YAZE_SDL2_TARGETS})
endif()

message(STATUS "Created ImGui target from bundled source at ${IMGUI_DIR}")

# Create ImGui Test Engine for test automation (if tests are enabled)
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
    if(YAZE_USE_SDL3)
      target_link_libraries(ImGuiTestEngine PUBLIC ImGui ${YAZE_SDL3_TARGETS})
    else()
      target_link_libraries(ImGuiTestEngine PUBLIC ImGui ${YAZE_SDL2_TARGETS})
    endif()
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
