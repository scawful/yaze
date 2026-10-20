# CMake project() requires a numeric version. The suffix belongs to display and
# package identity, including publish-disabled experimental builds.
if(DEFINED ENV{YAZE_VERSION_OVERRIDE})
    set(YAZE_VERSION "$ENV{YAZE_VERSION_OVERRIDE}")
elseif(DEFINED YAZE_VERSION_OVERRIDE)
    set(YAZE_VERSION "${YAZE_VERSION_OVERRIDE}")
else()
    set(_yaze_version_file "${CMAKE_CURRENT_LIST_DIR}/../VERSION")
    if(EXISTS "${_yaze_version_file}")
        file(READ "${_yaze_version_file}" YAZE_VERSION)
        string(STRIP "${YAZE_VERSION}" YAZE_VERSION)
    else()
        set(YAZE_VERSION "0.8.0")
    endif()
endif()

if(NOT DEFINED YAZE_VERSION_SUFFIX)
    set(YAZE_VERSION_SUFFIX "$ENV{YAZE_VERSION_SUFFIX}")
endif()
if(NOT YAZE_VERSION_SUFFIX STREQUAL "" AND
   (NOT YAZE_VERSION_SUFFIX MATCHES "^-[0-9A-Za-z-]+(\\.[0-9A-Za-z-]+)*$" OR
    YAZE_VERSION_SUFFIX MATCHES "-$"))
    message(FATAL_ERROR "Invalid YAZE_VERSION_SUFFIX: ${YAZE_VERSION_SUFFIX}")
endif()
