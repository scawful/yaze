#!/usr/bin/env bash

set -euo pipefail

TARGET_NAME=${1:-}
BUILD_DIR=${2:-}
ARTIFACT_NAME=${3:-}

if [[ -z "$TARGET_NAME" || -z "$BUILD_DIR" || -z "$ARTIFACT_NAME" ]]; then
  echo "Usage: release.sh <target-name> <build-dir> <artifact-name>" >&2
  exit 1
fi

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
ROOT_DIR=${GITHUB_WORKSPACE:-$(cd "$SCRIPT_DIR/../.." && pwd)}
ARTIFACTS_DIR="$ROOT_DIR/dist"
STAGING_DIR=$(mktemp -d)

mkdir -p "$ARTIFACTS_DIR"

cleanup() {
  rm -rf "$STAGING_DIR"
}
trap cleanup EXIT

echo "Packaging $TARGET_NAME using build output at $BUILD_DIR"

case "${RUNNER_OS:-$(uname)}" in
  Windows*)
    BIN_DIR="$BUILD_DIR/bin/Release"
    if [[ ! -d "$BIN_DIR" ]]; then
      echo "::error::Expected Windows binaries under $BIN_DIR" >&2
      exit 1
    fi
    cp -R "$BIN_DIR" "$STAGING_DIR/bin"
    cp -R "$ROOT_DIR/assets" "$STAGING_DIR/assets"
    cp "$ROOT_DIR"/LICENSE "$ROOT_DIR"/README.md "$STAGING_DIR"/
    ARTIFACT_PATH="$ARTIFACTS_DIR/$ARTIFACT_NAME"
    pwsh -NoLogo -NoProfile -Command "Compress-Archive -Path '${STAGING_DIR}\*' -DestinationPath '$ARTIFACT_PATH' -Force"
    ;;
  Darwin)
    APP_PATH="$BUILD_DIR/bin/yaze.app"
    if [[ ! -d "$APP_PATH" ]]; then
      echo "::error::Expected macOS app bundle at $APP_PATH" >&2
      exit 1
    fi
    cp -R "$APP_PATH" "$STAGING_DIR/yaze.app"
    cp "$ROOT_DIR"/LICENSE "$ROOT_DIR"/README.md "$STAGING_DIR"/
    ARTIFACT_PATH="$ARTIFACTS_DIR/$ARTIFACT_NAME"
    hdiutil create -fs HFS+ -srcfolder "$STAGING_DIR/yaze.app" -volname "yaze" "$ARTIFACT_PATH"
    ;;
  Linux*)
    BIN_DIR="$BUILD_DIR/bin"
    if [[ ! -d "$BIN_DIR" ]]; then
      echo "::error::Expected Linux binaries under $BIN_DIR" >&2
      exit 1
    fi
    cp "$ROOT_DIR"/LICENSE "$ROOT_DIR"/README.md "$STAGING_DIR"/
    cp -R "$BIN_DIR" "$STAGING_DIR/bin"
    cp -R "$ROOT_DIR/assets" "$STAGING_DIR/assets"
    ARTIFACT_PATH="$ARTIFACTS_DIR/$ARTIFACT_NAME"
    tar -czf "$ARTIFACT_PATH" -C "$STAGING_DIR" .
    ;;
  *)
    echo "::error::Unsupported host: ${RUNNER_OS:-$(uname)}" >&2
    exit 1
    ;;
esac

if [[ ! -f "$ARTIFACT_PATH" ]]; then
  echo "::error::Packaging did not produce $ARTIFACT_PATH" >&2
  exit 1
fi

if command -v sha256sum >/dev/null 2>&1; then
  SHA_CMD="sha256sum"
elif command -v shasum >/dev/null 2>&1; then
  SHA_CMD="shasum -a 256"
else
  echo "::warning::No SHA256 utility found; skipping checksum generation" >&2
  exit 0
fi

CHECKSUM=$($SHA_CMD "$ARTIFACT_PATH" | awk '{print $1}')
echo "$CHECKSUM  $(basename "$ARTIFACT_PATH")" >> "$ARTIFACTS_DIR/SHA256SUMS.txt"
echo "$CHECKSUM" > "$ARTIFACT_PATH.sha256"

echo "Created artifact $ARTIFACT_PATH"

