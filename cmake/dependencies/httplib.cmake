# cpp-httplib dependency management

if(EMSCRIPTEN)
  set(YAZE_HTTPLIB_TARGETS "" CACHE INTERNAL "cpp-httplib targets")
  return()
endif()

include(cmake/CPM.cmake)
include(cmake/dependencies.lock)

message(STATUS "Setting up cpp-httplib ${HTTPLIB_VERSION}")

set(_YAZE_USE_SYSTEM_HTTPLIB ${YAZE_USE_SYSTEM_DEPS})

# Try to use system packages first
if(_YAZE_USE_SYSTEM_HTTPLIB)
  find_package(httplib QUIET)
  if(httplib_FOUND)
    message(STATUS "Using system httplib")
    set(YAZE_HTTPLIB_TARGETS httplib::httplib CACHE INTERNAL "cpp-httplib targets")
    return()
  elseif(YAZE_USE_SYSTEM_DEPS)
    message(WARNING "System httplib not found despite YAZE_USE_SYSTEM_DEPS=ON; falling back to CPM download")
  endif()
endif()

CPMAddPackage(
  NAME httplib
  VERSION ${HTTPLIB_VERSION}
  GITHUB_REPOSITORY yhirose/cpp-httplib
  GIT_TAG v${HTTPLIB_VERSION}
  OPTIONS
    "HTTPLIB_INSTALL OFF"
    "HTTPLIB_TEST OFF"
    "HTTPLIB_COMPILE OFF"
    "HTTPLIB_USE_OPENSSL_IF_AVAILABLE OFF"
    "HTTPLIB_USE_ZLIB_IF_AVAILABLE OFF"
    "HTTPLIB_USE_BROTLI_IF_AVAILABLE OFF"
    "HTTPLIB_USE_ZSTD_IF_AVAILABLE OFF"
)

# Verify target is available
if(TARGET httplib::httplib)
  message(STATUS "httplib::httplib target found")
elseif(TARGET httplib)
  add_library(httplib::httplib ALIAS httplib)
  message(STATUS "Created httplib::httplib alias")
else()
  message(FATAL_ERROR "httplib target not found after CPM fetch")
endif()

set(YAZE_HTTPLIB_TARGETS httplib::httplib)
set(YAZE_HTTPLIB_TARGETS httplib::httplib CACHE INTERNAL "cpp-httplib targets")

message(STATUS "cpp-httplib setup complete")
