find_package(PythonLibs 3.8 REQUIRED)
cmake_policy(SET CMP0167 NEW)
find_package(Boost COMPONENTS python3 REQUIRED)

# target x86_64 for module 
add_library(
  yaze_py MODULE
  py/yaze_py.cc
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
  ${PYTHON_HEADERS}
  ${Boost_INCLUDE_DIRS}
  ${PYTHON_INCLUDE_DIRS}
)

target_link_libraries(
  yaze_py PUBLIC
  ${PYTHON_LIBRARIES}
  Boost::python3
)