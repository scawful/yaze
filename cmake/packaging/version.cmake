set(CPACK_PACKAGE_VERSION_MAJOR ${YAZE_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${YAZE_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${YAZE_VERSION_PATCH})
set(CPACK_PACKAGE_VERSION "${YAZE_VERSION_MAJOR}.${YAZE_VERSION_MINOR}.${YAZE_VERSION_PATCH}${YAZE_VERSION_SUFFIX}")
# Debian's '~' sorts a preview before its final release. The manifest and
# application retain the original tag spelling on every platform.
string(REGEX REPLACE "^([0-9]+\\.[0-9]+\\.[0-9]+)-" "\\1~"
    CPACK_DEBIAN_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION}")
