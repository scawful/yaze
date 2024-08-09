add_library(
  yaze_ext
  ext/extension.cc
)

target_include_directories(
  yaze_ext PUBLIC
  lib/
  app/
  ${CMAKE_SOURCE_DIR}/src/
  /Library/Developer/CommandLineTools/Library/Frameworks/Python3.framework/Versions/3.9/Headers
)

target_link_libraries(
  yaze_ext PUBLIC
  Python
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