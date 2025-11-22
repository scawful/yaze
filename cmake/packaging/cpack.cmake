# CPack Configuration
# Cross-platform packaging using CPack

include(CPack)

# Set package information
set(CPACK_PACKAGE_NAME "yaze")
set(CPACK_PACKAGE_VENDOR "scawful")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Yet Another Zelda3 Editor")
set(CPACK_PACKAGE_VERSION_MAJOR ${YAZE_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${YAZE_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${YAZE_VERSION_PATCH})
set(CPACK_PACKAGE_VERSION "${YAZE_VERSION_MAJOR}.${YAZE_VERSION_MINOR}.${YAZE_VERSION_PATCH}")

# Set package directory
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}/packages")

# Platform-specific packaging
if(APPLE)
  include(cmake/packaging/macos.cmake)
elseif(WIN32)
  include(cmake/packaging/windows.cmake)
elseif(UNIX)
  include(cmake/packaging/linux.cmake)
endif()

# Common files to include
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")

# Set default component
set(CPACK_COMPONENTS_ALL yaze)
set(CPACK_COMPONENT_YAZE_DISPLAY_NAME "YAZE Editor")
set(CPACK_COMPONENT_YAZE_DESCRIPTION "Main YAZE application and libraries")

# Install rules - these define what CPack packages
include(GNUInstallDirs)

# Windows uses a flat structure (no bin/, share/ subdirectories)
if(WIN32)
    set(YAZE_INSTALL_BINDIR ".")
    set(YAZE_INSTALL_DATADIR ".")
    set(YAZE_INSTALL_DOCDIR ".")
else()
    set(YAZE_INSTALL_BINDIR ${CMAKE_INSTALL_BINDIR})
    set(YAZE_INSTALL_DATADIR ${CMAKE_INSTALL_DATADIR})
    set(YAZE_INSTALL_DOCDIR ${CMAKE_INSTALL_DOCDIR})
endif()

# Install main executable
if(APPLE)
    install(TARGETS yaze
        RUNTIME DESTINATION ${YAZE_INSTALL_BINDIR}
        BUNDLE DESTINATION .
        COMPONENT yaze
    )
else()
    install(TARGETS yaze
        RUNTIME DESTINATION ${YAZE_INSTALL_BINDIR}
        COMPONENT yaze
    )
endif()

# Install assets
install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets/
    DESTINATION ${YAZE_INSTALL_DATADIR}/assets
    COMPONENT yaze
    PATTERN "*.png"
    PATTERN "*.ttf"
    PATTERN "*.asm"
)

# Install documentation
install(FILES
    ${CMAKE_SOURCE_DIR}/README.md
    ${CMAKE_SOURCE_DIR}/LICENSE
    DESTINATION ${YAZE_INSTALL_DOCDIR}
    COMPONENT yaze
)

