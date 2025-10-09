# vcpkg configuration reporting for Windows builds
# This file is included AFTER vcpkg toolchain has run, so we can only report and validate
# 
# IMPORTANT: vcpkg configuration variables (VCPKG_TARGET_TRIPLET, etc.) must be set:
#   1. On the CMake command line: -DVCPKG_TARGET_TRIPLET=x64-windows-static
#   2. Via environment variables: set VCPKG_DEFAULT_TRIPLET=x64-windows-static
#   3. In vcpkg-configuration.json in the project root

# Windows-specific macro definitions to avoid conflicts
add_definitions("-DMICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS=0")

# Determine what triplet is being used (for reporting)
set(_vcpkg_triplet "unknown")
if(DEFINED VCPKG_TARGET_TRIPLET)
    set(_vcpkg_triplet "${VCPKG_TARGET_TRIPLET}")
elseif(DEFINED ENV{VCPKG_DEFAULT_TRIPLET})
    set(_vcpkg_triplet "$ENV{VCPKG_DEFAULT_TRIPLET}")
endif()

# Detect installed directory
set(_vcpkg_installed "${CMAKE_BINARY_DIR}/vcpkg_installed")
if(DEFINED VCPKG_INSTALLED_DIR)
    set(_vcpkg_installed "${VCPKG_INSTALLED_DIR}")
endif()

# Detect manifest mode
set(_vcpkg_manifest "ON (auto)")
if(DEFINED VCPKG_MANIFEST_MODE)
    if(VCPKG_MANIFEST_MODE)
        set(_vcpkg_manifest "ON")
    else()
        set(_vcpkg_manifest "OFF")
    endif()
endif()

# Report vcpkg configuration
message(STATUS "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
message(STATUS "vcpkg Post-Toolchain Report:")
message(STATUS "  ├─ Active triplet: ${_vcpkg_triplet}")
message(STATUS "  ├─ Manifest mode: ${_vcpkg_manifest}")
message(STATUS "  └─ Installed directory: ${_vcpkg_installed}")
message(STATUS "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")

# Validation warnings
if(_vcpkg_triplet STREQUAL "unknown")
    message(WARNING "vcpkg triplet not detected! Build may fail.")
    message(WARNING "Set VCPKG_TARGET_TRIPLET on command line or VCPKG_DEFAULT_TRIPLET env var")
endif()

# Ensure manifest file exists
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/vcpkg.json")
    message(WARNING "vcpkg.json manifest not found in ${CMAKE_SOURCE_DIR}")
    message(WARNING "vcpkg dependency installation may fail!")
endif()
