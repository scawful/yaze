#!/bin/bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <dmg-path>"
  exit 2
fi

dmg_path="$1"

# Handle glob expansion that didn't match any files
if [[ "$dmg_path" == *"*"* ]]; then
  echo "No DMG files found (glob pattern not expanded): $dmg_path"
  exit 2
fi

if [[ ! -f "$dmg_path" ]]; then
  echo "DMG not found: $dmg_path"
  echo "Contents of parent directory:"
  ls -la "$(dirname "$dmg_path")" 2>/dev/null || echo "Directory doesn't exist"
  exit 2
fi

echo "Validating DMG: $dmg_path"
echo "File size: $(ls -lh "$dmg_path" | awk '{print $5}')"

mount_dir="$(mktemp -d)"
cleanup() {
  hdiutil detach "$mount_dir" -quiet 2>/dev/null || true
  rm -rf "$mount_dir"
}
trap cleanup EXIT

echo "Mounting DMG..."
mount_output=$(hdiutil attach "$dmg_path" -mountpoint "$mount_dir" -nobrowse 2>&1) || {
  echo "Failed to mount DMG"
  echo "hdiutil output: $mount_output"
  echo "Checking DMG file integrity..."
  file "$dmg_path" || true
  hdiutil imageinfo "$dmg_path" 2>&1 | head -20 || true
  exit 1
}

echo "DMG contents:"
ls -la "$mount_dir/" || true

required=(
  "README.md"
  "LICENSE"
  "manifest.json"
  "assets"
  "yaze.app"
  "z3ed"
)

missing_items=()
for item in "${required[@]}"; do
  if [[ ! -e "$mount_dir/$item" ]]; then
    missing_items+=("$item")
  fi
done

if [[ ${#missing_items[@]} -gt 0 ]]; then
  echo "Missing required items in DMG:"
  for item in "${missing_items[@]}"; do
    echo "  - $item"
  done
  exit 1
fi

if [[ -d "$mount_dir/bin" ]]; then
  echo "Unexpected bin/ directory in DMG"
  exit 1
fi

helpers=(
  "overworld_golden_data_extractor"
  "extract_vanilla_values"
  "rom_patch_utility"
  "dungeon_test_harness"
)

for helper in "${helpers[@]}"; do
  if find "$mount_dir" \( -name "$helper" -o -name "${helper}.exe" \) -print | grep -q .; then
    echo "Unexpected helper tool in DMG: $helper"
    exit 1
  fi
done

echo "DMG validation passed."
