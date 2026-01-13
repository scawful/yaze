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
set(YAZE_RELEASE_README "${CMAKE_SOURCE_DIR}/docs/public/release/README.md")
set(CPACK_RESOURCE_FILE_README "${YAZE_RELEASE_README}")

set(CPACK_COMPONENTS_ALL yaze)
set(CPACK_COMPONENT_YAZE_DISPLAY_NAME "YAZE Editor")
set(CPACK_COMPONENT_YAZE_DESCRIPTION "Main YAZE application and libraries")

function(yaze_manifest_bool input_var output_var)
    if(DEFINED ${input_var})
        if(${input_var})
            set(${output_var} "true" PARENT_SCOPE)
        else()
            set(${output_var} "false" PARENT_SCOPE)
        endif()
    else()
        set(${output_var} "false" PARENT_SCOPE)
    endif()
endfunction()

set(YAZE_MANIFEST_VERSION "${CPACK_PACKAGE_VERSION}")
set(YAZE_MANIFEST_BUILD_TYPE "${CMAKE_BUILD_TYPE}")
if(NOT YAZE_MANIFEST_BUILD_TYPE)
    if(CMAKE_CONFIGURATION_TYPES)
        set(YAZE_MANIFEST_BUILD_TYPE "MultiConfig")
    else()
        set(YAZE_MANIFEST_BUILD_TYPE "unknown")
    endif()
endif()

if(DEFINED CMAKE_PRESET_NAME)
    set(YAZE_MANIFEST_PRESET "${CMAKE_PRESET_NAME}")
else()
    set(YAZE_MANIFEST_PRESET "unknown")
endif()

execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE YAZE_MANIFEST_GIT_SHA
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT YAZE_MANIFEST_GIT_SHA)
    set(YAZE_MANIFEST_GIT_SHA "unknown")
endif()

string(TIMESTAMP YAZE_MANIFEST_BUILD_TIMESTAMP "%Y-%m-%dT%H:%M:%SZ" UTC)
set(YAZE_MANIFEST_GENERATOR "${CMAKE_GENERATOR}")
set(YAZE_MANIFEST_CXX_COMPILER_ID "${CMAKE_CXX_COMPILER_ID}")
set(YAZE_MANIFEST_CXX_COMPILER_VERSION "${CMAKE_CXX_COMPILER_VERSION}")

yaze_manifest_bool(YAZE_BUILD_GUI YAZE_MANIFEST_BUILD_GUI)
yaze_manifest_bool(YAZE_BUILD_Z3ED YAZE_MANIFEST_BUILD_Z3ED)
yaze_manifest_bool(YAZE_BUILD_CLI YAZE_MANIFEST_BUILD_CLI)
yaze_manifest_bool(YAZE_ENABLE_AI YAZE_MANIFEST_ENABLE_AI)
yaze_manifest_bool(YAZE_ENABLE_AI_RUNTIME YAZE_MANIFEST_ENABLE_AI_RUNTIME)
yaze_manifest_bool(YAZE_ENABLE_GRPC YAZE_MANIFEST_ENABLE_GRPC)
yaze_manifest_bool(YAZE_ENABLE_HTTP_API YAZE_MANIFEST_ENABLE_HTTP_API)
yaze_manifest_bool(YAZE_ENABLE_AGENT_CLI YAZE_MANIFEST_ENABLE_AGENT_CLI)
yaze_manifest_bool(YAZE_ENABLE_JSON YAZE_MANIFEST_ENABLE_JSON)

set(YAZE_MANIFEST_TEMPLATE "${CMAKE_SOURCE_DIR}/cmake/packaging/manifest.json.in")
set(YAZE_RELEASE_MANIFEST "${CMAKE_BINARY_DIR}/manifest.json")
configure_file(${YAZE_MANIFEST_TEMPLATE} ${YAZE_RELEASE_MANIFEST} @ONLY)

# Populate runtime library list (needed on Windows)
set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_NO_WARNINGS ON)
set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP ON)
set(CMAKE_INSTALL_UCRT_LIBRARIES OFF)
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
        ${YAZE_RELEASE_README}
        ${YAZE_RELEASE_MANIFEST}
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
        ${YAZE_RELEASE_README}
        ${YAZE_RELEASE_MANIFEST}
        ${CMAKE_SOURCE_DIR}/LICENSE
        DESTINATION .
        COMPONENT yaze)

    # Bundle MSVC/UCRT runtime dependencies if detected
    if(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS)
        list(REMOVE_DUPLICATES CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS)
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
        ${YAZE_RELEASE_README}
        ${YAZE_RELEASE_MANIFEST}
        ${CMAKE_SOURCE_DIR}/LICENSE
        DESTINATION .
        COMPONENT yaze)
endif()

include(CPack)
