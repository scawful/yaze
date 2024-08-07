add_library(
  yaze_ext
  ext/extension.cc
)

target_include_directories(
  yaze_ext PUBLIC
  lib/
  app/
  /Library/Developer/CommandLineTools/Library/Frameworks/Python3.framework/Versions/3.9/Headers
)

target_link_libraries(
  yaze_ext PUBLIC
  Python
)