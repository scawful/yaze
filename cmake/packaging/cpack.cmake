# CPack Configuration - flat packages for all platforms
#
# Structure:
#   root/
#     yaze(.exe)
#     z3ed(.exe)           (if built)
#     README.md
#     LICENSE
#     assets/...

set(CPACK_PACKAGE_NAME "yaze")
set(CPACK_PACKAGE_VENDOR "scawful")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Yet Another Zelda3 Editor")
set(CPACK_PACKAGE_VERSION_MAJOR ${YAZE_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${YAZE_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${YAZE_VERSION_PATCH})
set(CPACK_PACKAGE_VERSION "${YAZE_VERSION_MAJOR}.${YAZE_VERSION_MINOR}.${YAZE_VERSION_PATCH}")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}/packages")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")

set(CPACK_COMPONENTS_ALL yaze)
set(CPACK_COMPONENT_YAZE_DISPLAY_NAME "YAZE Editor")
set(CPACK_COMPONENT_YAZE_DESCRIPTION "Main YAZE application and libraries")

# Populate runtime library list (needed on Windows)
set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_NO_WARNINGS ON)
set(CMAKE_INSTALL_UCRT_LIBRARIES ON)
include(InstallRequiredSystemLibraries)

if(APPLE)
    include(cmake/packaging/macos.cmake)

    install(TARGETS yaze
        BUNDLE DESTINATION .
        COMPONENT yaze)

    if(TARGET z3ed)
        install(TARGETS z3ed
            RUNTIME DESTINATION .
            COMPONENT yaze)
    endif()

    install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets/
        DESTINATION assets
        COMPONENT yaze)

    install(FILES
        ${CMAKE_SOURCE_DIR}/README.md
        ${CMAKE_SOURCE_DIR}/LICENSE
        DESTINATION .
        COMPONENT yaze)

elseif(WIN32)
    include(cmake/packaging/windows.cmake)

    install(TARGETS yaze
        RUNTIME DESTINATION .
        COMPONENT yaze)

    if(TARGET z3ed)
        install(TARGETS z3ed
            RUNTIME DESTINATION .
            COMPONENT yaze)
    endif()

    install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets/
        DESTINATION assets
        COMPONENT yaze)

    install(FILES
        ${CMAKE_SOURCE_DIR}/README.md
        ${CMAKE_SOURCE_DIR}/LICENSE
        DESTINATION .
        COMPONENT yaze)

    if(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS)
        install(FILES ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS}
            DESTINATION .
            COMPONENT yaze)
    endif()

else()
    include(cmake/packaging/linux.cmake)

    install(TARGETS yaze
        RUNTIME DESTINATION .
        COMPONENT yaze)

    if(TARGET z3ed)
        install(TARGETS z3ed
            RUNTIME DESTINATION .
            COMPONENT yaze)
    endif()

    install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets/
        DESTINATION assets
        COMPONENT yaze)

    install(FILES
        ${CMAKE_SOURCE_DIR}/README.md
        ${CMAKE_SOURCE_DIR}/LICENSE
        DESTINATION .
        COMPONENT yaze)
endif()

include(CPack)
