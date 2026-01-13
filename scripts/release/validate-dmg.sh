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

mount_dir="$(mktemp -d)"
cleanup() {
  hdiutil detach "$mount_dir" -quiet || true
  rm -rf "$mount_dir"
}
trap cleanup EXIT

hdiutil attach "$dmg_path" -mountpoint "$mount_dir" -nobrowse -quiet

required=(
  "README.md"
  "LICENSE"
  "manifest.json"
  "assets"
  "yaze.app"
  "z3ed"
)

for item in "${required[@]}"; do
  if [[ ! -e "$mount_dir/$item" ]]; then
    echo "Missing $item in DMG"
    exit 1
  fi
done

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
