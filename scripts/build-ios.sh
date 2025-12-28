#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PRESET="${1:-ios-debug}"

cmake --preset "${PRESET}"
cmake --build --preset "${PRESET}"

if ! command -v xcodegen >/dev/null 2>&1; then
  echo "xcodegen not found. Install with: brew install xcodegen" >&2
  exit 1
fi

(cd "${ROOT_DIR}/src/ios" && xcodegen)

echo "Generated Xcode project at ${ROOT_DIR}/src/ios/yaze_ios.xcodeproj"
