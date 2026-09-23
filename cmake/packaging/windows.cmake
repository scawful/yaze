# Windows Packaging Configuration

# NSIS installer
set(CPACK_GENERATOR "NSIS;ZIP")
set(CPACK_NSIS_PACKAGE_NAME "YAZE Editor")
set(CPACK_NSIS_DISPLAY_NAME "YAZE Editor v${CPACK_PACKAGE_VERSION}")
set(CPACK_NSIS_CONTACT "scawful")
set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/scawful/yaze")
set(CPACK_NSIS_HELP_LINK "https://github.com/scawful/yaze")
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "yaze ${CPACK_PACKAGE_VERSION}")

# CPack uses this filename for both the NSIS installer and ZIP archive.
set(_yaze_windows_target
    "${CMAKE_GENERATOR_PLATFORM};${CMAKE_SYSTEM_PROCESSOR}")
string(TOLOWER "${_yaze_windows_target}" _yaze_windows_target)
if(_yaze_windows_target MATCHES "arm64|aarch64")
    set(_yaze_windows_arch "arm64")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(_yaze_windows_arch "x64")
else()
    set(_yaze_windows_arch "x86")
endif()
set(CPACK_PACKAGE_FILE_NAME
    "yaze-${CPACK_PACKAGE_VERSION}-windows-${_yaze_windows_arch}")
unset(_yaze_windows_arch)
unset(_yaze_windows_target)

# Authenticode signing is not configured here. The current release workflow
# emits unsigned Windows artifacts and treats signing as a separate release gate.
