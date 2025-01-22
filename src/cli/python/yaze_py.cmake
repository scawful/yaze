find_package(PythonLibs 3.11 REQUIRED)
find_package(Boost COMPONENTS python3 REQUIRED)

# target x86_64 for module 
add_library(
  yaze_py MODULE
  py/yaze_py.cc
  yaze.cc
  app/rom.cc
  app/core/labeling.cc
  app/zelda3/overworld/overworld_map.cc
  app/zelda3/overworld/overworld.cc
  app/zelda3/sprite/sprite.cc
  app/editor/utils/gfx_context.cc
  ${YAZE_APP_GFX_SRC}
  ${IMGUI_PATH}/imgui.cpp
  ${IMGUI_PATH}/imgui_demo.cpp
  ${IMGUI_PATH}/imgui_draw.cpp
  ${IMGUI_PATH}/imgui_widgets.cpp
  ${IMGUI_PATH}/misc/cpp/imgui_stdlib.cpp
)

if (APPLE)
  set(PYTHON_HEADERS /Library/Developer/CommandLineTools/Library/Frameworks/Python3.framework/Versions/3.9/Headers)
elseif(LINUX)
  set(PYTHON_HEADERS /usr/include/python3.8)
endif()

target_include_directories(
  yaze_py PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/
  lib/
  app/
  ${SDL_INCLUDE_DIR}
  ${PYTHON_HEADERS}
  ${Boost_INCLUDE_DIRS}
  ${PYTHON_INCLUDE_DIRS}
)

target_link_libraries(
  yaze_py PUBLIC
  ${SDL_TARGETS}
  ${ABSL_TARGETS}
  ${PYTHON_LIBRARIES}
  ${PNG_LIBRARIES}
  Boost::python3
  ImGui
  ImGuiTestEngine
)