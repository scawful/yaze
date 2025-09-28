
# vcpkg configuration for Windows builds
add_definitions("-DMICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS=0")

# vcpkg settings
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)

# Enable vcpkg manifest mode for automatic dependency management
set(VCPKG_MANIFEST_MODE ON)

# Auto-detect target architecture and set vcpkg triplet
if(NOT DEFINED VCPKG_TARGET_TRIPLET)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
        set(VCPKG_TARGET_TRIPLET "arm64-windows" CACHE STRING "vcpkg target triplet")
        set(VCPKG_TARGET_ARCHITECTURE arm64)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64")
        set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "vcpkg target triplet")
        set(VCPKG_TARGET_ARCHITECTURE x64)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i386|i686|x86")
        set(VCPKG_TARGET_TRIPLET "x86-windows" CACHE STRING "vcpkg target triplet")
        set(VCPKG_TARGET_ARCHITECTURE x86)
    else()
        # Fallback to x64 if architecture detection fails
        set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "vcpkg target triplet")
        set(VCPKG_TARGET_ARCHITECTURE x64)
        message(WARNING "Could not detect target architecture, defaulting to x64")
    endif()
endif()

# Set vcpkg installation directory if not already set
if(NOT DEFINED VCPKG_INSTALLED_DIR)
    set(VCPKG_INSTALLED_DIR "${CMAKE_BINARY_DIR}/vcpkg_installed" CACHE PATH "vcpkg installed directory")
endif()

message(STATUS "vcpkg configuration:")
message(STATUS "  Target architecture: ${VCPKG_TARGET_ARCHITECTURE}")
message(STATUS "  Target triplet: ${VCPKG_TARGET_TRIPLET}")
message(STATUS "  Installed directory: ${VCPKG_INSTALLED_DIR}")
message(STATUS "  Manifest mode: ${VCPKG_MANIFEST_MODE}")
