#!/usr/bin/env bash
# Source checks only: this does not build the C++ core or qualify a device.
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"
check_dir="$(mktemp -d "${TMPDIR:-/tmp}/yaze-ios-review.XXXXXX")"
trap 'rm -rf "$check_dir"' EXIT

xcrun swiftc -parse-as-library \
  src/ios/iOS/DesktopAPIClient.swift \
  src/ios/iOS/DesktopDiscoveryService.swift \
  test/ios/desktop_api_connection_harness.swift \
  -o "$check_dir/desktop-connection-tests"
"$check_dir/desktop-connection-tests"

sdk_path="$(xcrun --sdk iphonesimulator --show-sdk-path)"
swift_sources=()
while IFS= read -r -d '' source_file; do
  swift_sources+=("$source_file")
done < <(find src/ios/iOS -name '*.swift' -print0)
xcrun --sdk iphonesimulator swiftc -typecheck \
  -target arm64-apple-ios17.0-simulator -sdk "$sdk_path" \
  -import-objc-header src/ios/iOS/Yaze-Bridging-Header.h \
  "${swift_sources[@]}"
echo 'PASS: native iOS Swift source type checking (not a linked app build)'
