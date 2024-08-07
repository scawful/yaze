find_package(Python3 COMPONENTS Interpreter Development REQUIRED)

add_library(
  yaze_python
  py/yaze_python.cc
)

set(PYTHON_HEADERS /Library/Developer/CommandLineTools/Library/Frameworks/Python3.framework/Versions/3.9/Headers)

target_include_directories(
  yaze_python PUBLIC
  lib/
  app/
  ${PYTHON_HEADERS}
)

target_link_libraries(
  yaze_python PUBLIC
  Python
)