# CPM.cmake - C++ Package Manager
# https://github.com/cpm-cmake/CPM.cmake

set(CPM_DOWNLOAD_VERSION 0.38.7)

if(CPM_SOURCE_CACHE)
  set(CPM_DOWNLOAD_LOCATION "${CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
elseif(DEFINED ENV{CPM_SOURCE_CACHE})
  set(CPM_DOWNLOAD_LOCATION "$ENV{CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
else()
  set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
endif()

# Expand relative path. This is important if the provided path contains a tilde (~)
get_filename_component(CPM_DOWNLOAD_LOCATION ${CPM_DOWNLOAD_LOCATION} ABSOLUTE)

function(download_cpm)
  message(STATUS "Downloading CPM.cmake to ${CPM_DOWNLOAD_LOCATION}")
  file(DOWNLOAD
       https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake
       ${CPM_DOWNLOAD_LOCATION}
  )
endfunction()

if(NOT (EXISTS ${CPM_DOWNLOAD_LOCATION}))
  download_cpm()
else()
  # resume download if it previously failed
  file(READ ${CPM_DOWNLOAD_LOCATION} check)
  if("${check}" STREQUAL "")
    download_cpm()
  endif()
endif()

include(${CPM_DOWNLOAD_LOCATION})

# Set CPM options for better caching and performance
set(CPM_USE_LOCAL_PACKAGES ON)
set(CPM_LOCAL_PACKAGES_ONLY OFF)
set(CPM_DONT_CREATE_PACKAGE_LOCK ON)
set(CPM_DONT_UPDATE_MODULE_PATH ON)
set(CPM_DONT_PREPEND_TO_MODULE_PATH ON)

# Set cache directory for CI builds
if(DEFINED ENV{GITHUB_ACTIONS})
  set(CPM_SOURCE_CACHE "$ENV{HOME}/.cpm-cache")
  message(STATUS "CPM cache directory: ${CPM_SOURCE_CACHE}")
endif()
