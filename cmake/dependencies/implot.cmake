# ImPlot dependency management
# Uses the bundled ImPlot sources that ship with the ImGui Test Engine

set(YAZE_IMPLOT_TARGETS "")

set(IMPLOT_DIR ${CMAKE_SOURCE_DIR}/ext/imgui_test_engine/imgui_test_suite/thirdparty/implot)

if(EXISTS ${IMPLOT_DIR}/implot.h)
  message(STATUS "Setting up ImPlot from bundled sources")

  add_library(ImPlot STATIC
    ${IMPLOT_DIR}/implot.cpp
    ${IMPLOT_DIR}/implot_items.cpp
  )

  target_include_directories(ImPlot PUBLIC
    ${IMPLOT_DIR}
    ${IMGUI_DIR}
  )

  target_link_libraries(ImPlot PUBLIC ImGui)
  target_compile_features(ImPlot PUBLIC cxx_std_17)

  set(YAZE_IMPLOT_TARGETS ImPlot)
else()
  message(WARNING "ImPlot sources not found at ${IMPLOT_DIR}. Plot widgets will be unavailable.")
endif()
