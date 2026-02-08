#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

usage() {
  cat <<'EOF'
Usage:
  scripts/xcodebuild-ios.sh [PRESET] [ACTION]

PRESET:
  ios-debug        (default) device Debug
  ios-release                 device Release
  ios-sim-debug               simulator Debug

ACTION:
  build            (default) build the app
  archive                     create an .xcarchive (device presets only)
  ipa                         archive + export a development .ipa (device presets only)

Signing notes (device builds):
  If your Xcode Accounts are broken/unavailable, you can still let xcodebuild
  manage provisioning by passing an App Store Connect auth key:

    export XCODE_AUTH_KEY_PATH=/path/to/AuthKey_XXXXXX.p8
    export XCODE_AUTH_KEY_ID=XXXXXX
    export XCODE_AUTH_KEY_ISSUER_ID=XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX

  Then run:
    scripts/xcodebuild-ios.sh ios-debug ipa
EOF
}

PRESET="${1:-ios-debug}"
ACTION="${2:-build}"

case "${ACTION}" in
  build|archive|ipa) ;;
  -h|--help|help) usage; exit 0 ;;
  *) echo "Unknown ACTION: ${ACTION}" >&2; usage; exit 2 ;;
esac

"${ROOT_DIR}/scripts/build-ios.sh" "${PRESET}"

XCODE_PROJECT="${ROOT_DIR}/src/ios/yaze_ios.xcodeproj"
XCODE_SCHEME="yaze_ios"

CONFIG="Debug"
if [[ "${PRESET}" == *release* ]]; then
  CONFIG="Release"
fi

DERIVED_DATA="${ROOT_DIR}/build/xcode/derived/${XCODE_SCHEME}-${PRESET}"
mkdir -p "${DERIVED_DATA}"

AUTH_ARGS=()
if [[ -n "${XCODE_AUTH_KEY_PATH:-}" ]]; then
  : "${XCODE_AUTH_KEY_ID:?Set XCODE_AUTH_KEY_ID when using XCODE_AUTH_KEY_PATH}"
  : "${XCODE_AUTH_KEY_ISSUER_ID:?Set XCODE_AUTH_KEY_ISSUER_ID when using XCODE_AUTH_KEY_PATH}"
  AUTH_ARGS+=(
    -authenticationKeyPath "${XCODE_AUTH_KEY_PATH}"
    -authenticationKeyID "${XCODE_AUTH_KEY_ID}"
    -authenticationKeyIssuerID "${XCODE_AUTH_KEY_ISSUER_ID}"
  )
fi

COMMON_ARGS=(
  -project "${XCODE_PROJECT}"
  -scheme "${XCODE_SCHEME}"
  -configuration "${CONFIG}"
  -derivedDataPath "${DERIVED_DATA}"
)

timestamp() {
  date "+%Y%m%d-%H%M%S"
}

if [[ "${PRESET}" == ios-sim-* ]]; then
  if [[ "${ACTION}" != "build" ]]; then
    echo "ACTION '${ACTION}' is only supported for device presets." >&2
    exit 2
  fi

  HOST_ARCH="$(uname -m)"
  xcodebuild \
    "${COMMON_ARGS[@]}" \
    -sdk iphonesimulator \
    -destination "generic/platform=iOS Simulator" \
    ONLY_ACTIVE_ARCH=YES \
    ARCHS="${HOST_ARCH}" \
    CODE_SIGNING_ALLOWED=NO \
    CODE_SIGNING_REQUIRED=NO \
    build
  exit 0
fi

# Device build/signing.
PROVISIONING_ARGS=(
  -allowProvisioningUpdates
  -allowProvisioningDeviceRegistration
  "${AUTH_ARGS[@]}"
)

case "${ACTION}" in
  build)
    xcodebuild \
      "${COMMON_ARGS[@]}" \
      -sdk iphoneos \
      -destination "generic/platform=iOS" \
      "${PROVISIONING_ARGS[@]}" \
      build
    ;;
  archive|ipa)
    ARCHIVE_DIR="${ROOT_DIR}/build/xcode/archives"
    mkdir -p "${ARCHIVE_DIR}"
    ARCHIVE_PATH="${ARCHIVE_DIR}/${XCODE_SCHEME}-${PRESET}-$(timestamp).xcarchive"

    xcodebuild \
      "${COMMON_ARGS[@]}" \
      -sdk iphoneos \
      -destination "generic/platform=iOS" \
      -archivePath "${ARCHIVE_PATH}" \
      "${PROVISIONING_ARGS[@]}" \
      archive

    if [[ "${ACTION}" == "ipa" ]]; then
      EXPORT_DIR="${ROOT_DIR}/build/xcode/exports/${XCODE_SCHEME}-${PRESET}-$(timestamp)"
      mkdir -p "${EXPORT_DIR}"

      EXPORT_PLIST="${ROOT_DIR}/build/xcode/ExportOptions-${XCODE_SCHEME}.plist"
      cat >"${EXPORT_PLIST}" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>method</key>
  <string>debugging</string>
  <key>signingStyle</key>
  <string>automatic</string>
</dict>
</plist>
EOF

      xcodebuild \
        -exportArchive \
        -archivePath "${ARCHIVE_PATH}" \
        -exportPath "${EXPORT_DIR}" \
        -exportOptionsPlist "${EXPORT_PLIST}" \
        "${PROVISIONING_ARGS[@]}"
    fi
    ;;
esac
