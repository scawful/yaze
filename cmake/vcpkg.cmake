
# vcpkg configuration for Windows builds
add_definitions("-DMICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS=0")

# vcpkg settings
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)

# Enable vcpkg manifest mode for automatic dependency management
set(VCPKG_MANIFEST_MODE ON)

# Configure vcpkg triplet (defaults to x64-windows)
if(NOT DEFINED VCPKG_TARGET_TRIPLET)
    set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "vcpkg target triplet")
endif()

# Set vcpkg installation directory if not already set
if(NOT DEFINED VCPKG_INSTALLED_DIR)
    set(VCPKG_INSTALLED_DIR "${CMAKE_BINARY_DIR}/vcpkg_installed" CACHE PATH "vcpkg installed directory")
endif()

message(STATUS "vcpkg configuration:")
message(STATUS "  Target triplet: ${VCPKG_TARGET_TRIPLET}")
message(STATUS "  Installed directory: ${VCPKG_INSTALLED_DIR}")
message(STATUS "  Manifest mode: ${VCPKG_MANIFEST_MODE}")
