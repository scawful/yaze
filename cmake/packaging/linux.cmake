# Linux Packaging Configuration

# DEB package
set(CPACK_GENERATOR "DEB;TGZ")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "scawful")
set(CPACK_DEBIAN_PACKAGE_SECTION "games")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6, libstdc++6, libsdl2-2.0-0")

# RPM package
set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Games")
set(CPACK_RPM_PACKAGE_REQUIRES "glibc, libstdc++, SDL2")

# Tarball
set(CPACK_TGZ_PACKAGE_NAME "yaze-${CPACK_PACKAGE_VERSION}-linux-x64")
