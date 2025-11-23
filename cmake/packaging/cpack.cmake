# CPack Configuration
# Cross-platform packaging using CPack with FLAT structure
#
# Package structure:
#   root/
#     yaze (or yaze.exe)
#     z3ed (or z3ed.exe) - if CLI is built
#     README.md
#     LICENSE
#     assets/
#       (all asset files)

# Set package information BEFORE including CPack
set(CPACK_PACKAGE_NAME "yaze")
set(CPACK_PACKAGE_VENDOR "scawful")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Yet Another Zelda3 Editor")
set(CPACK_PACKAGE_VERSION_MAJOR ${YAZE_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${YAZE_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${YAZE_VERSION_PATCH})
set(CPACK_PACKAGE_VERSION "${YAZE_VERSION_MAJOR}.${YAZE_VERSION_MINOR}.${YAZE_VERSION_PATCH}")

# Set package directory
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}/packages")

# Common files to include
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")

# Set default component
set(CPACK_COMPONENTS_ALL yaze)
set(CPACK_COMPONENT_YAZE_DISPLAY_NAME "YAZE Editor")
set(CPACK_COMPONENT_YAZE_DESCRIPTION "Main YAZE application and libraries")

# =============================================================================
# FLAT PACKAGE STRUCTURE INSTALL RULES
# =============================================================================
# We explicitly avoid GNUInstallDirs to create a simple, flat package structure
# that users can extract and run directly without installation.

if(APPLE)
    # -------------------------------------------------------------------------
    # macOS: Create app bundle with embedded resources
    # -------------------------------------------------------------------------
    # Platform-specific CPack configuration
    include(cmake/packaging/macos.cmake)

    # Install the app bundle to the root of the package
    install(TARGETS yaze
        BUNDLE DESTINATION .
        COMPONENT yaze
    )

    # Install z3ed CLI alongside the bundle (if built)
    if(TARGET z3ed)
        install(TARGETS z3ed
            RUNTIME DESTINATION .
            COMPONENT yaze
        )
    endif()

    # Install assets into the bundle's Resources folder
    # This is done via the bundle's resource handling, but we also
    # install to a root assets/ folder for CLI tools
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets/
        DESTINATION assets
        COMPONENT yaze
    )

    # Install documentation to root
    install(FILES
        ${CMAKE_SOURCE_DIR}/README.md
        ${CMAKE_SOURCE_DIR}/LICENSE
        DESTINATION .
        COMPONENT yaze
    )

    # Bundle MSVC/UCRT runtime dependencies if available
    if(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS)
        install(FILES ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS}
            DESTINATION .
            COMPONENT yaze
        )
    endif()

    # Bundle MSVC/UCRT runtime dependencies if available
    if(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS)
        install(FILES ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS}
            DESTINATION .
            COMPONENT yaze
        )
    endif()

    # Bundle MSVC/UCRT runtime dependencies if available
    if(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS)
        install(FILES ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS}
            DESTINATION .
            COMPONENT yaze
        )
    endif()

    # Bundle MSVC/UCRT runtime dependencies if available
    if(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS)
        install(FILES ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS}
            DESTINATION .
            COMPONENT yaze
        )
    endif()

elseif(WIN32)
    # -------------------------------------------------------------------------
    # Windows: Flat structure with executables at root
    # -------------------------------------------------------------------------
    include(cmake/packaging/windows.cmake)

    # Install executables to root (no bin/ subdirectory)
    install(TARGETS yaze
        RUNTIME DESTINATION .
        COMPONENT yaze
    )

    # Install z3ed CLI (if built)
    if(TARGET z3ed)
        install(TARGETS z3ed
            RUNTIME DESTINATION .
            COMPONENT yaze
        )
    endif()

    # Install assets to assets/ folder at root
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets/
        DESTINATION assets
        COMPONENT yaze
    )

    # Install documentation to root
    install(FILES
        ${CMAKE_SOURCE_DIR}/README.md
        ${CMAKE_SOURCE_DIR}/LICENSE
        DESTINATION .
        COMPONENT yaze
    )

else()
    # -------------------------------------------------------------------------
    # Linux: Flat structure for tar.gz, FHS for DEB/RPM
    # -------------------------------------------------------------------------
    include(cmake/packaging/linux.cmake)

    # For the TGZ generator, use flat structure
    # For DEB/RPM, use FHS-compliant structure
    # CPack handles this via generator-specific install prefixes

    # Install executables to root (flat) or bin/ (FHS)
    install(TARGETS yaze
        RUNTIME DESTINATION .
        COMPONENT yaze
    )

    # Install z3ed CLI (if built)
    if(TARGET z3ed)
        install(TARGETS z3ed
            RUNTIME DESTINATION .
            COMPONENT yaze
        )
    endif()

    # Install assets to assets/ folder
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets/
        DESTINATION assets
        COMPONENT yaze
    )

    # Install documentation to root
    install(FILES
        ${CMAKE_SOURCE_DIR}/README.md
        ${CMAKE_SOURCE_DIR}/LICENSE
        DESTINATION .
        COMPONENT yaze
    )
endif()

# Include CPack module AFTER all configuration
include(CPack)
