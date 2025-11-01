# Windows Packaging Configuration

# NSIS installer
set(CPACK_GENERATOR "NSIS;ZIP")
set(CPACK_NSIS_PACKAGE_NAME "YAZE Editor")
set(CPACK_NSIS_DISPLAY_NAME "YAZE Editor v${CPACK_PACKAGE_VERSION}")
set(CPACK_NSIS_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION}")
set(CPACK_NSIS_CONTACT "scawful")
set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/scawful/yaze")
set(CPACK_NSIS_HELP_LINK "https://github.com/scawful/yaze")
set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/scawful/yaze")
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)

# ZIP package
set(CPACK_ZIP_PACKAGE_NAME "yaze-${CPACK_PACKAGE_VERSION}-windows-x64")

# Code signing (if available)
if(DEFINED ENV{SIGNTOOL_CERTIFICATE})
  set(CPACK_NSIS_SIGN_TOOL "signtool.exe")
  set(CPACK_NSIS_SIGN_COMMAND "${ENV{SIGNTOOL_CERTIFICATE}}")
endif()
