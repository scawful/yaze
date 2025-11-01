# Dear ImGui dependency management
# Uses CPM.cmake for consistent cross-platform builds

include(cmake/CPM.cmake)
include(cmake/dependencies.lock)

message(STATUS "Setting up Dear ImGui ${IMGUI_VERSION} with CPM.cmake")

# Use CPM to fetch Dear ImGui
CPMAddPackage(
  NAME imgui
  VERSION ${IMGUI_VERSION}
  GITHUB_REPOSITORY ocornut/imgui
  GIT_TAG v${IMGUI_VERSION}
  OPTIONS
    "IMGUI_BUILD_EXAMPLES OFF"
    "IMGUI_BUILD_DEMO OFF"
)

# ImGui doesn't create targets during CPM fetch, they're created during build
# We'll create our own interface target and link to ImGui when it's available
add_library(yaze_imgui INTERFACE)

# Note: ImGui targets will be available after the build phase
# For now, we'll create a placeholder that can be linked later

# Add platform-specific backends
if(TARGET imgui_impl_sdl2)
  target_link_libraries(yaze_imgui INTERFACE imgui_impl_sdl2)
endif()

if(TARGET imgui_impl_opengl3)
  target_link_libraries(yaze_imgui INTERFACE imgui_impl_opengl3)
endif()

# Export ImGui targets for use in other CMake files
set(YAZE_IMGUI_TARGETS yaze_imgui)

message(STATUS "Dear ImGui setup complete")
