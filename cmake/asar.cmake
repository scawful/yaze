# Modern Asar 65816 Assembler Integration
# Improved cross-platform support for macOS, Linux, and Windows

# Include guard to prevent multiple inclusions
if(ASAR_CMAKE_INCLUDED)
  return()
endif()
set(ASAR_CMAKE_INCLUDED TRUE)

# Configure Asar build options
# Build the standalone executable so we can fall back to a bundled CLI when the
# static library misbehaves.
set(ASAR_GEN_EXE ON CACHE BOOL "Build Asar standalone executable")
set(ASAR_GEN_DLL ON CACHE BOOL "Build Asar shared library")
set(ASAR_GEN_LIB ON CACHE BOOL "Build Asar static library")
set(ASAR_GEN_EXE_TEST OFF CACHE BOOL "Build Asar executable tests")
set(ASAR_GEN_DLL_TEST OFF CACHE BOOL "Build Asar DLL tests")

# Force Asar to use dynamic MSVC runtime to match the app runtime
if(MSVC)
    set(MSVC_LIB_TYPE D CACHE STRING "Asar MSVC runtime type" FORCE)
endif()

# Set Asar source directory
set(ASAR_SRC_DIR "${CMAKE_SOURCE_DIR}/ext/asar/src")

# Add Asar as subdirectory with explicit binary directory
add_subdirectory(${ASAR_SRC_DIR} ${CMAKE_BINARY_DIR}/asar EXCLUDE_FROM_ALL)

# Create modern CMake target for Asar integration
if(TARGET asar-static)
    # Ensure asar-static is available and properly configured
    set_target_properties(asar-static PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        POSITION_INDEPENDENT_CODE ON
    )
    
    # Set platform-specific definitions for Asar
    if(WIN32)
        target_compile_definitions(asar-static PRIVATE
            windows
            strncasecmp=_strnicmp
            strcasecmp=_stricmp
            _CRT_SECURE_NO_WARNINGS
            _CRT_NONSTDC_NO_WARNINGS
        )
    elseif(UNIX AND NOT APPLE)
        target_compile_definitions(asar-static PRIVATE
            linux
            stricmp=strcasecmp
        )
    elseif(YAZE_PLATFORM_MACOS)
        target_compile_definitions(asar-static PRIVATE
            MACOS
            stricmp=strcasecmp
        )
    elseif(YAZE_PLATFORM_IOS)
        target_compile_definitions(asar-static PRIVATE
            YAZE_IOS
            stricmp=strcasecmp
        )
    endif()
    
    # Add include directories
    target_include_directories(asar-static PUBLIC
        $<BUILD_INTERFACE:${ASAR_SRC_DIR}>
        $<BUILD_INTERFACE:${ASAR_SRC_DIR}/asar>
        $<BUILD_INTERFACE:${ASAR_SRC_DIR}/asar-dll-bindings/c>
    )
    
    # Create alias for easier linking
    add_library(yaze::asar ALIAS asar-static)
    
    # Export Asar variables for use in other parts of the build
    set(ASAR_FOUND TRUE CACHE BOOL "Asar library found")
    set(ASAR_LIBRARIES asar-static CACHE STRING "Asar library target")
    set(ASAR_INCLUDE_DIRS 
        "${ASAR_SRC_DIR}"
        "${ASAR_SRC_DIR}/asar"
        "${ASAR_SRC_DIR}/asar-dll-bindings/c"
        CACHE STRING "Asar include directories"
    )
    
    message(STATUS "Asar 65816 assembler integration configured successfully")
else()
    message(WARNING "Failed to configure Asar static library target")
    set(ASAR_FOUND FALSE CACHE BOOL "Asar library found")
endif()

# Function to add Asar patching capabilities to a target
function(yaze_add_asar_support target_name)
    if(ASAR_FOUND)
        target_link_libraries(${target_name} PRIVATE yaze::asar)
        target_include_directories(${target_name} PRIVATE ${ASAR_INCLUDE_DIRS})
        target_compile_definitions(${target_name} PRIVATE YAZE_ENABLE_ASAR=1)
    else()
        message(WARNING "Asar not available for target ${target_name}")
    endif()
endfunction()

# Create function for ROM patching utilities
function(yaze_create_asar_patch_tool tool_name patch_file rom_file)
    if(ASAR_FOUND)
        add_custom_target(${tool_name}
            COMMAND ${CMAKE_COMMAND} -E echo "Patching ROM with Asar..."
            COMMAND $<TARGET_FILE:asar-standalone> ${patch_file} ${rom_file}
            DEPENDS asar-standalone
            COMMENT "Applying Asar patch ${patch_file} to ${rom_file}"
        )
    endif()
endfunction()
