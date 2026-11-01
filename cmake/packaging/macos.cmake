# macOS Packaging Configuration

# Create .dmg package
set(CPACK_GENERATOR "DragNDrop")
set(CPACK_DMG_VOLUME_NAME "yaze")
set(CPACK_DMG_FORMAT "UDZO")
set(CPACK_DMG_BACKGROUND_IMAGE "${CMAKE_SOURCE_DIR}/assets/yaze.png")
# Disable SLA popup when mounting DMG in CI.
set(CPACK_DMG_SLA_USE_RESOURCE_FILE_LICENSE OFF)

# App bundle configuration
set(CPACK_BUNDLE_NAME "yaze")
set(CPACK_BUNDLE_PACKAGE_TYPE "APPL")
set(CPACK_BUNDLE_ICON "${CMAKE_SOURCE_DIR}/assets/yaze.icns")

# Copy Homebrew dylibs (openssl@3, libpng) into yaze.app/Contents/Frameworks
# and re-sign, so the DMG runs without Homebrew.
set(CPACK_YAZE_BUNDLE_DYLIBS_SCRIPT
    "${CMAKE_SOURCE_DIR}/scripts/release/bundle-macos-dylibs.sh")
set(CPACK_PRE_BUILD_SCRIPTS
    "${CMAKE_CURRENT_LIST_DIR}/macos_bundle_dylibs.cmake")

# Code signing (if available)
if(DEFINED ENV{CODESIGN_IDENTITY})
  set(CPACK_BUNDLE_APPLE_CERT_APP "$ENV{CODESIGN_IDENTITY}")
  set(CPACK_BUNDLE_APPLE_CODESIGN_FORCE "ON")
endif()

# Notarization (if available)
if(DEFINED ENV{NOTARIZATION_CREDENTIALS})
  set(CPACK_BUNDLE_APPLE_NOTARIZATION_CREDENTIALS "$ENV{NOTARIZATION_CREDENTIALS}")
endif()
