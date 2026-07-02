# Linux Packaging Configuration

# --- Portability guardrail -------------------------------------------------
# A Linux package (.deb/.tar.gz) is only portable across distributions when the
# protobuf/gRPC stack is statically linked from source. When gRPC is enabled and
# a *system* protobuf/gRPC is preferred, the app binaries gain a hard dependency
# on the build host's libprotobuf SONAME (e.g. libprotobuf.so.23 on Ubuntu
# 20.04/22.04), which does not exist on another distribution (e.g. Debian 13) and
# fails at runtime with "libprotobuf.so.NN: cannot open shared object file".
# The default build (and the `release` preset) links protobuf statically, so this
# only trips when YAZE_PREFER_SYSTEM_GRPC / YAZE_USE_SYSTEM_DEPS are turned ON.
if(NOT DEFINED YAZE_STRICT_PORTABLE_PACKAGING)
  option(YAZE_STRICT_PORTABLE_PACKAGING
    "Fail configure when packaging a non-portable (system-linked protobuf) Linux build" OFF)
endif()
if(YAZE_ENABLE_GRPC AND (YAZE_PREFER_SYSTEM_GRPC OR YAZE_USE_SYSTEM_DEPS))
  set(_yaze_nonportable_pkg_msg
"Packaging a build that links the SYSTEM protobuf/gRPC stack \
(YAZE_PREFER_SYSTEM_GRPC/YAZE_USE_SYSTEM_DEPS=ON). The resulting .deb/.tar.gz \
will NOT be portable across Linux distributions: the yaze/z3ed binaries hard-link \
the build host's libprotobuf SONAME and fail elsewhere with \
'libprotobuf.so.NN: cannot open shared object file'. For redistributable packages, \
configure with -DYAZE_PREFER_SYSTEM_GRPC=OFF -DYAZE_USE_SYSTEM_DEPS=OFF (the \
default; protobuf is then statically linked from source), as the 'release' preset \
does. Set -DYAZE_STRICT_PORTABLE_PACKAGING=ON to make this a hard error.")
  if(YAZE_STRICT_PORTABLE_PACKAGING)
    message(FATAL_ERROR "${_yaze_nonportable_pkg_msg}")
  else()
    message(WARNING "${_yaze_nonportable_pkg_msg}")
  endif()
  unset(_yaze_nonportable_pkg_msg)
endif()

# DEB package
set(CPACK_GENERATOR "DEB;TGZ")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "scawful")
set(CPACK_DEBIAN_PACKAGE_SECTION "games")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6, libstdc++6, libsdl2-2.0-0")

# Auto-compute the real runtime library dependencies via dpkg-shlibdeps and merge
# them with the hand-maintained base list above. This keeps the .deb honest: a
# build that links protobuf/gRPC dynamically (YAZE_PREFER_SYSTEM_GRPC=ON) then
# declares its libprotobuf/libgrpc dependency, so apt/dpkg refuses to install it
# on an incompatible distribution instead of failing at runtime with
# "libprotobuf.so.NN: cannot open shared object file". For the default static
# release build this adds no protobuf/gRPC entries (nothing to depend on).
# Requires dpkg-dev (dpkg-shlibdeps) on the packaging host.
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

# RPM package
set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Games")
set(CPACK_RPM_PACKAGE_REQUIRES "glibc, libstdc++, SDL2")

# Tarball
set(CPACK_TGZ_PACKAGE_NAME "yaze-${CPACK_PACKAGE_VERSION}-linux-x64")

