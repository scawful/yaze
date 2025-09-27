# Modern packaging configuration for Yaze
# Supports Windows (NSIS), macOS (DMG), and Linux (DEB/RPM)

include(InstallRequiredSystemLibraries)

# Basic package information
set(CPACK_PACKAGE_NAME "yaze")
set(CPACK_PACKAGE_VENDOR "scawful")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Yet Another Zelda3 Editor")
set(CPACK_PACKAGE_DESCRIPTION "A comprehensive editor for The Legend of Zelda: A Link to the Past ROM hacking")
set(CPACK_PACKAGE_VERSION_MAJOR ${YAZE_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${YAZE_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${YAZE_VERSION_PATCH})
set(CPACK_PACKAGE_VERSION "${YAZE_VERSION_MAJOR}.${YAZE_VERSION_MINOR}.${YAZE_VERSION_PATCH}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Yaze")
set(CPACK_PACKAGE_CONTACT "scawful@github.com")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/scawful/yaze")

# Resource files
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")

# Package icon
if(EXISTS "${CMAKE_SOURCE_DIR}/assets/yaze.png")
    set(CPACK_PACKAGE_ICON "${CMAKE_SOURCE_DIR}/assets/yaze.png")
endif()

# Platform-specific configuration
if(WIN32)
    # Windows packaging configuration (conditional based on environment)
    if(DEFINED ENV{GITHUB_ACTIONS})
        # CI/CD build - use only ZIP (NSIS not available)
        set(CPACK_GENERATOR "ZIP")
    else()
        # Local build - use both NSIS installer and ZIP
        set(CPACK_GENERATOR "NSIS;ZIP")
    endif()
    
    # NSIS-specific configuration (only for local builds with NSIS available)
    if(NOT DEFINED ENV{GITHUB_ACTIONS})
        set(CPACK_NSIS_DISPLAY_NAME "Yaze - Zelda3 Editor")
        set(CPACK_NSIS_PACKAGE_NAME "Yaze")
        set(CPACK_NSIS_CONTACT "scawful@github.com")
        set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/scawful/yaze")
        set(CPACK_NSIS_HELP_LINK "https://github.com/scawful/yaze/issues")
        set(CPACK_NSIS_MENU_LINKS
            "bin/yaze.exe" "Yaze Editor"
            "https://github.com/scawful/yaze" "Yaze Homepage"
        )
        set(CPACK_NSIS_CREATE_ICONS_EXTRA
            "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Yaze.lnk' '$INSTDIR\\\\bin\\\\yaze.exe'"
            "CreateShortCut '$DESKTOP\\\\Yaze.lnk' '$INSTDIR\\\\bin\\\\yaze.exe'"
        )
        set(CPACK_NSIS_DELETE_ICONS_EXTRA
            "Delete '$SMPROGRAMS\\\\$START_MENU\\\\Yaze.lnk'"
            "Delete '$DESKTOP\\\\Yaze.lnk'"
        )
    endif()
    
    # Windows architecture detection
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        if(DEFINED ENV{GITHUB_ACTIONS})
            set(CPACK_PACKAGE_FILE_NAME "yaze-${CPACK_PACKAGE_VERSION}-windows-x64")
        else()
            set(CPACK_PACKAGE_FILE_NAME "yaze-${CPACK_PACKAGE_VERSION}-win64")
        endif()
        set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
    else()
        if(DEFINED ENV{GITHUB_ACTIONS})
            set(CPACK_PACKAGE_FILE_NAME "yaze-${CPACK_PACKAGE_VERSION}-windows-x86")
        else()
            set(CPACK_PACKAGE_FILE_NAME "yaze-${CPACK_PACKAGE_VERSION}-win32")
        endif()
        set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES")
    endif()
    
elseif(APPLE)
    # macOS DMG configuration
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "Yaze ${CPACK_PACKAGE_VERSION}")
    set(CPACK_DMG_FORMAT "UDZO")
    set(CPACK_PACKAGE_FILE_NAME "yaze-${CPACK_PACKAGE_VERSION}-macos")
    
    # macOS app bundle configuration
    if(EXISTS "${CMAKE_SOURCE_DIR}/assets/dmg_background.png")
        set(CPACK_DMG_BACKGROUND_IMAGE "${CMAKE_SOURCE_DIR}/assets/dmg_background.png")
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/cmake/dmg_setup.scpt")
        set(CPACK_DMG_DS_STORE_SETUP_SCRIPT "${CMAKE_SOURCE_DIR}/cmake/dmg_setup.scpt")
    endif()
    
elseif(UNIX)
    # Linux DEB/RPM configuration
    set(CPACK_GENERATOR "DEB;RPM;TGZ")
    
    # DEB package configuration
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "scawful <scawful@github.com>")
    set(CPACK_DEBIAN_PACKAGE_SECTION "games")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS 
        "libsdl2-2.0-0, libpng16-16, libgl1-mesa-glx, libabsl20210324")
    set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "git")
    set(CPACK_DEBIAN_PACKAGE_SUGGESTS "asar")
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    
    # RPM package configuration
    set(CPACK_RPM_PACKAGE_SUMMARY "Zelda3 ROM Editor")
    set(CPACK_RPM_PACKAGE_LICENSE "MIT")
    set(CPACK_RPM_PACKAGE_GROUP "Amusements/Games")
    set(CPACK_RPM_PACKAGE_REQUIRES 
        "SDL2 >= 2.0.0, libpng >= 1.6.0, mesa-libGL, abseil-cpp")
    set(CPACK_RPM_PACKAGE_SUGGESTS "asar")
    set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
    
    # Architecture detection
    execute_process(
        COMMAND uname -m
        OUTPUT_VARIABLE CPACK_SYSTEM_ARCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(CPACK_PACKAGE_FILE_NAME "yaze-${CPACK_PACKAGE_VERSION}-linux-${CPACK_SYSTEM_ARCH}")
endif()

# Component configuration for advanced packaging
set(CPACK_COMPONENTS_ALL applications libraries headers documentation)

set(CPACK_COMPONENT_APPLICATIONS_DISPLAY_NAME "Yaze Application")
set(CPACK_COMPONENT_APPLICATIONS_DESCRIPTION "Main Yaze editor application")
set(CPACK_COMPONENT_APPLICATIONS_REQUIRED TRUE)

set(CPACK_COMPONENT_LIBRARIES_DISPLAY_NAME "Development Libraries")
set(CPACK_COMPONENT_LIBRARIES_DESCRIPTION "Yaze development libraries")
set(CPACK_COMPONENT_LIBRARIES_REQUIRED FALSE)

set(CPACK_COMPONENT_HEADERS_DISPLAY_NAME "Development Headers")
set(CPACK_COMPONENT_HEADERS_DESCRIPTION "Header files for Yaze development")
set(CPACK_COMPONENT_HEADERS_REQUIRED FALSE)
set(CPACK_COMPONENT_HEADERS_DEPENDS libraries)

set(CPACK_COMPONENT_DOCUMENTATION_DISPLAY_NAME "Documentation")
set(CPACK_COMPONENT_DOCUMENTATION_DESCRIPTION "User and developer documentation")
set(CPACK_COMPONENT_DOCUMENTATION_REQUIRED FALSE)

# Installation components
if(APPLE)
    install(TARGETS yaze
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        BUNDLE DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT applications
    )
else()
    install(TARGETS yaze
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT applications
    )
endif()

# Install assets
install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/yaze/assets
    COMPONENT applications
    PATTERN "*.png"
    PATTERN "*.ttf"
    PATTERN "*.asm"
)

# Install documentation
install(FILES 
    ${CMAKE_SOURCE_DIR}/README.md
    ${CMAKE_SOURCE_DIR}/LICENSE
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
    COMPONENT documentation
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/docs/
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
    COMPONENT documentation
    PATTERN "*.md"
    PATTERN "*.html"
)

# Install headers and libraries if building library components
if(YAZE_INSTALL_LIB)
    install(TARGETS yaze_c
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        COMPONENT libraries
    )
    
    install(FILES ${CMAKE_SOURCE_DIR}/incl/yaze.h ${CMAKE_SOURCE_DIR}/incl/zelda.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/yaze
        COMPONENT headers
    )
endif()

# Desktop integration for Linux
if(UNIX AND NOT APPLE)
    # Desktop file
    configure_file(
        ${CMAKE_SOURCE_DIR}/cmake/yaze.desktop.in
        ${CMAKE_BINARY_DIR}/yaze.desktop
        @ONLY
    )
    
    install(FILES ${CMAKE_BINARY_DIR}/yaze.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
        COMPONENT applications
    )
    
    # Icon
    if(EXISTS "${CMAKE_SOURCE_DIR}/assets/yaze.png")
        install(FILES ${CMAKE_SOURCE_DIR}/assets/yaze.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/pixmaps
            RENAME yaze.png
            COMPONENT applications
        )
    endif()
endif()

# Include CPack
include(CPack)
