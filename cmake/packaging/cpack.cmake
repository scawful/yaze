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

