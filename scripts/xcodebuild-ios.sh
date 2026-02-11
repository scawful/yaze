#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Optional local overrides (gitignored).
SIGNING_ENV="${ROOT_DIR}/scripts/signing.env"
if [[ -f "${SIGNING_ENV}" ]]; then
  # shellcheck disable=SC1090
  source "${SIGNING_ENV}"
fi

usage() {
  cat <<'EOF'
Usage:
  scripts/xcodebuild-ios.sh [PRESET] [ACTION] [DEVICE]

PRESET:
  ios-debug        (default) device Debug
  ios-release                 device Release
  ios-sim-debug               simulator Debug

ACTION:
  build            (default) build the app
  archive                     create an .xcarchive (device presets only)
  ipa                         archive + export a development .ipa (device presets only)
  deploy                      build + install to a paired device (device presets only)

Signing notes (device builds):
  If your Xcode Accounts are broken/unavailable, you can still let xcodebuild
  manage provisioning by passing an App Store Connect auth key:

    export XCODE_AUTH_KEY_PATH=/path/to/AuthKey_XXXXXX.p8
    export XCODE_AUTH_KEY_ID=XXXXXX
    export XCODE_AUTH_KEY_ISSUER_ID=XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX

  Then run:
    scripts/xcodebuild-ios.sh ios-debug ipa

Identifier overrides (optional):
  Use these if you need to sign with your own team/bundle ID (recommended for
  local device builds; upstream IDs may not be available on your account).

    export YAZE_IOS_TEAM_ID=YOUR_TEAM_ID
    export YAZE_IOS_BUNDLE_ID=com.yourcompany.yaze-ios
    export YAZE_ICLOUD_CONTAINER_ID="iCloud.${YAZE_IOS_BUNDLE_ID}"

Deploy defaults:
  DEVICE defaults to $YAZE_IOS_DEVICE, then "Baby Pad".
  Set YAZE_IOS_LAUNCH_AFTER_DEPLOY=0 to skip auto-launch after install.
EOF
}

PRESET="${1:-ios-debug}"
ACTION="${2:-build}"
DEVICE="${3:-${YAZE_IOS_DEVICE:-Baby Pad}}"

case "${ACTION}" in
  build|archive|ipa|deploy) ;;
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

SETTING_OVERRIDES=()
if [[ -n "${YAZE_IOS_TEAM_ID:-}" ]]; then
  SETTING_OVERRIDES+=(YAZE_IOS_TEAM_ID="${YAZE_IOS_TEAM_ID}")
fi
if [[ -n "${YAZE_IOS_BUNDLE_ID:-}" ]]; then
  SETTING_OVERRIDES+=(YAZE_IOS_BUNDLE_ID="${YAZE_IOS_BUNDLE_ID}")
fi
if [[ -n "${YAZE_ICLOUD_CONTAINER_ID:-}" ]]; then
  SETTING_OVERRIDES+=(YAZE_ICLOUD_CONTAINER_ID="${YAZE_ICLOUD_CONTAINER_ID}")
fi

timestamp() {
  date "+%Y%m%d-%H%M%S"
}

resolve_device_app_path() {
  local products_dir="$1"
  local preferred_path="${products_dir}/${XCODE_SCHEME}.app"
  if [[ -d "${preferred_path}" ]]; then
    printf '%s\n' "${preferred_path}"
    return 0
  fi

  mapfile -t app_bundles < <(find "${products_dir}" -maxdepth 1 -type d -name "*.app" | sort)

  if [[ ${#app_bundles[@]} -eq 0 ]]; then
    echo "No app bundle found in ${products_dir}" >&2
    return 1
  fi

  for app in "${app_bundles[@]}"; do
    if [[ "$(basename "${app}")" == "yaze.app" ]]; then
      printf '%s\n' "${app}"
      return 0
    fi
  done

  if [[ ${#app_bundles[@]} -eq 1 ]]; then
    printf '%s\n' "${app_bundles[0]}"
    return 0
  fi

  echo "Multiple app bundles found; specify scheme-specific resolution." >&2
  printf '  %s\n' "${app_bundles[@]}" >&2
  return 1
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
    "${SETTING_OVERRIDES[@]}" \
    build
  exit 0
fi

# Device build/signing.
PROVISIONING_ARGS=(
  -allowProvisioningUpdates
  -allowProvisioningDeviceRegistration
  "${AUTH_ARGS[@]}"
)

IOS_BUNDLE_ID="${YAZE_IOS_BUNDLE_ID:-org.halext.yaze-ios}"
IOS_PRODUCTS_DIR="${DERIVED_DATA}/Build/Products/${CONFIG}-iphoneos"

case "${ACTION}" in
  build)
    xcodebuild \
      "${COMMON_ARGS[@]}" \
      -sdk iphoneos \
      -destination "generic/platform=iOS" \
      "${PROVISIONING_ARGS[@]}" \
      "${SETTING_OVERRIDES[@]}" \
      build
    ;;
  deploy)
    xcodebuild \
      "${COMMON_ARGS[@]}" \
      -sdk iphoneos \
      -destination "generic/platform=iOS" \
      "${PROVISIONING_ARGS[@]}" \
      "${SETTING_OVERRIDES[@]}" \
      build

    if ! DEVICE_APP_PATH="$(resolve_device_app_path "${IOS_PRODUCTS_DIR}")"; then
      exit 1
    fi

    if ! xcrun -f devicectl >/dev/null 2>&1; then
      echo "xcrun devicectl not found; install/update Xcode command line tools." >&2
      exit 1
    fi

    echo "Installing ${DEVICE_APP_PATH} to device: ${DEVICE}"
    xcrun devicectl device install app --device "${DEVICE}" "${DEVICE_APP_PATH}"

    if [[ "${YAZE_IOS_LAUNCH_AFTER_DEPLOY:-1}" != "0" ]]; then
      echo "Launching ${IOS_BUNDLE_ID} on device: ${DEVICE}"
      xcrun devicectl device process launch \
        --device "${DEVICE}" \
        --terminate-existing \
        "${IOS_BUNDLE_ID}" || true
    fi
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
      "${SETTING_OVERRIDES[@]}" \
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
