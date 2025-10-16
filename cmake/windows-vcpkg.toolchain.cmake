# Windows vcpkg toolchain wrapper
# This file provides a convenient way to configure vcpkg for Windows builds
#
# Usage:
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/windows-vcpkg.toolchain.cmake ..
#
# Or set VCPKG_ROOT environment variable and this will find it automatically

# Set vcpkg triplet for static Windows builds
set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "vcpkg triplet")
set(VCPKG_HOST_TRIPLET "x64-windows" CACHE STRING "vcpkg host triplet")

# Enable manifest mode
set(VCPKG_MANIFEST_MODE ON CACHE BOOL "Use vcpkg manifest mode")

# Find vcpkg root
if(DEFINED ENV{VCPKG_ROOT} AND EXISTS "$ENV{VCPKG_ROOT}")
  set(VCPKG_ROOT "$ENV{VCPKG_ROOT}" CACHE PATH "vcpkg root directory")
elseif(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../vcpkg/scripts/buildsystems/vcpkg.cmake")
  set(VCPKG_ROOT "${CMAKE_CURRENT_LIST_DIR}/../vcpkg" CACHE PATH "vcpkg root directory")
else()
  message(WARNING "vcpkg not found. Set VCPKG_ROOT environment variable or clone vcpkg to project root.")
  message(WARNING "  git clone https://github.com/Microsoft/vcpkg.git")
  message(WARNING "  cd vcpkg && bootstrap-vcpkg.bat")
  return()
endif()

# Include the vcpkg toolchain
set(VCPKG_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
if(EXISTS "${VCPKG_TOOLCHAIN_FILE}")
  message(STATUS "Using vcpkg toolchain: ${VCPKG_TOOLCHAIN_FILE}")
  message(STATUS "  Triplet: ${VCPKG_TARGET_TRIPLET}")
  include("${VCPKG_TOOLCHAIN_FILE}")
else()
  message(FATAL_ERROR "vcpkg toolchain not found at ${VCPKG_TOOLCHAIN_FILE}")
endif()

