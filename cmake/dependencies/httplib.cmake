if(DEFINED YAZE_HTTPLIB_CMAKE_INCLUDED)
  return()
endif()
set(YAZE_HTTPLIB_CMAKE_INCLUDED TRUE)

set(_yaze_httplib_version "0.16.3")
set(_yaze_httplib_bundle_dir "${CMAKE_SOURCE_DIR}/ext/httplib")

set(_yaze_httplib_source_dir "")

if(EXISTS "${_yaze_httplib_bundle_dir}/httplib.h")
  message(NOTICE "Setting up cpp-httplib from ext/httplib bundle")
  set(_yaze_httplib_source_dir "${_yaze_httplib_bundle_dir}")
else()
  message(NOTICE "ext/httplib missing; fetching cpp-httplib ${_yaze_httplib_version} via FetchContent")
  include(FetchContent)
  FetchContent_Declare(
    httplib_external
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG        v${_yaze_httplib_version}
    GIT_SHALLOW    TRUE
  )
  FetchContent_MakeAvailable(httplib_external)
  set(_yaze_httplib_source_dir "${httplib_external_SOURCE_DIR}")
endif()

if(NOT TARGET yaze_httplib)
  add_library(yaze_httplib INTERFACE)
endif()

target_include_directories(yaze_httplib INTERFACE
  $<BUILD_INTERFACE:${_yaze_httplib_source_dir}>
  $<INSTALL_INTERFACE:include>
)

set(YAZE_HTTPLIB_INCLUDE_DIR "${_yaze_httplib_source_dir}" CACHE INTERNAL "cpp-httplib include directory")
set(YAZE_HTTPLIB_TARGETS yaze_httplib CACHE INTERNAL "cpp-httplib interface target")

message(STATUS "cpp-httplib setup complete - include dir: ${YAZE_HTTPLIB_INCLUDE_DIR}")
