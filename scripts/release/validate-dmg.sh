#!/bin/bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <dmg-path>"
  exit 2
fi

dmg_path="$1"
if [[ ! -f "$dmg_path" ]]; then
  echo "DMG not found: $dmg_path"
  exit 2
fi

echo "Validating DMG: $dmg_path"

mount_dir="$(mktemp -d)"
cleanup() {
  hdiutil detach "$mount_dir" -quiet 2>/dev/null || true
  rm -rf "$mount_dir"
}
trap cleanup EXIT

echo "Mounting DMG..."
if ! hdiutil attach "$dmg_path" -mountpoint "$mount_dir" -nobrowse -quiet 2>&1; then
  echo "Failed to mount DMG"
  exit 1
fi

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
