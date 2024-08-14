add_library(
  yaze_ext
  ext/extension.cc
)

target_include_directories(
  yaze_ext PUBLIC
  lib/
  app/
  ${CMAKE_SOURCE_DIR}/src/
  ${Boost_INCLUDE_DIRS}
)

target_link_libraries(
  yaze_ext PUBLIC
  ${PYTHON_LIBRARIES}
  Boost::python3
)

# C Sample 
add_library(
  yaze_ext_c SHARED
  ext/sample.c
)

target_include_directories(
  yaze_ext_c PUBLIC
  lib/
  ${CMAKE_SOURCE_DIR}/src/
)

target_link_libraries(
  yaze_ext_c PUBLIC
  yaze_ext
)