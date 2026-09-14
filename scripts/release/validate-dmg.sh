#!/bin/bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <dmg-path> --expected-version <version> [--expected-git-sha <sha>]"
  exit 2
fi

dmg_path="$1"
shift

expected_version=""
expected_git_sha=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --expected-version)
      [[ $# -ge 2 ]] || { echo "Missing value for --expected-version"; exit 2; }
      expected_version="$2"
      shift 2
      ;;
    --expected-git-sha)
      [[ $# -ge 2 ]] || { echo "Missing value for --expected-git-sha"; exit 2; }
      expected_git_sha="$2"
      shift 2
      ;;
    *)
      echo "Unknown argument: $1"
      exit 2
      ;;
  esac
done

if [[ -z "$expected_version" ]]; then
  echo "--expected-version is required"
  exit 2
fi

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
isolated_dir="$(mktemp -d)"
cleanup() {
  hdiutil detach "$mount_dir" -quiet 2>/dev/null || true
  rm -rf "$mount_dir" "$isolated_dir"
}
trap cleanup EXIT

echo "Mounting DMG..."
mount_output=$(hdiutil attach "$dmg_path" -mountpoint "$mount_dir" -nobrowse -readonly -noverify 2>&1) || {
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

if [[ ! -d "$mount_dir/assets" ]] || [[ -z "$(find "$mount_dir/assets" -type f -print -quit)" ]]; then
  echo "DMG assets directory is missing or empty"
  exit 1
fi

bundle_assets="$mount_dir/yaze.app/Contents/Resources/assets"
if [[ ! -d "$bundle_assets" ]] || [[ -z "$(find "$bundle_assets" -type f -print -quit)" ]]; then
  echo "yaze.app embedded assets directory is missing or empty"
  exit 1
fi
if ! diff -qr "$mount_dir/assets" "$bundle_assets"; then
  echo "yaze.app embedded assets do not match the complete DMG asset tree"
  exit 1
fi

python3 - "$mount_dir/manifest.json" "$expected_version" "$expected_git_sha" <<'PY'
import json
import pathlib
import sys

manifest_path = pathlib.Path(sys.argv[1])
expected_version = sys.argv[2].removeprefix("v")
expected_git_sha = sys.argv[3].lower()

with manifest_path.open(encoding="utf-8") as handle:
    manifest = json.load(handle)

for key in ("name", "version", "git_sha", "features"):
    if key not in manifest:
        raise SystemExit(f"manifest.json missing '{key}'")
if not isinstance(manifest["features"], dict):
    raise SystemExit("manifest.json 'features' must be an object")

actual_version = str(manifest["version"]).removeprefix("v")
if expected_version and actual_version != expected_version:
    raise SystemExit(
        f"manifest.json version mismatch: expected {expected_version}, "
        f"found {actual_version}"
    )

actual_git_sha = str(manifest["git_sha"]).lower()
if expected_git_sha and (
    len(actual_git_sha) < 7 or not expected_git_sha.startswith(actual_git_sha)
):
    raise SystemExit(
        f"manifest.json git_sha mismatch: expected prefix of {expected_git_sha}, "
        f"found {actual_git_sha}"
    )
PY

echo "Validating yaze.app code signature..."
if ! codesign --verify --deep --strict "$mount_dir/yaze.app"; then
  echo "Invalid code signature for yaze.app"
  codesign -dv --verbose=2 "$mount_dir/yaze.app" 2>&1 || true
  exit 1
fi

isolated_app="$isolated_dir/yaze.app"
/usr/bin/ditto "$mount_dir/yaze.app" "$isolated_app"
if ! codesign --verify --deep --strict "$isolated_app"; then
  echo "Relocated yaze.app has an invalid code signature"
  codesign -dv --verbose=2 "$isolated_app" 2>&1 || true
  exit 1
fi

if [[ ! -x "$mount_dir/yaze.app/Contents/MacOS/yaze" ]]; then
  echo "yaze.app is missing executable Contents/MacOS/yaze"
  exit 1
fi

if [[ ! -x "$mount_dir/z3ed" ]]; then
  echo "z3ed is not executable"
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

run_smoke_test() {
  local label="$1"
  local working_directory="$2"
  local executable="$3"
  local argument="$4"
  local expected_output="${5:-}"

  python3 - "$label" "$working_directory" "$executable" "$argument" "$expected_output" <<'PY'
import subprocess
import sys

label, working_directory, executable, argument, expected_output = sys.argv[1:]
try:
    result = subprocess.run(
        [executable, argument],
        cwd=working_directory,
        capture_output=True,
        text=True,
        timeout=30,
        check=False,
    )
except subprocess.TimeoutExpired:
    raise SystemExit(f"{label} did not exit within 30 seconds")

output = result.stdout + result.stderr
if output:
    print(output, end="" if output.endswith("\n") else "\n")
if result.returncode != 0:
    raise SystemExit(f"{label} failed with status {result.returncode}")
if not output.strip():
    raise SystemExit(f"{label} produced no output")
if expected_output and output.strip() != expected_output:
    raise SystemExit(
        f"{label} output mismatch: expected {expected_output!r}, "
        f"found {output.strip()!r}"
    )
PY
}

echo "Smoke-testing packaged executables..."
run_smoke_test \
  "relocated yaze --version" \
  "$isolated_dir" \
  "$isolated_app/Contents/MacOS/yaze" \
  --version \
  "yaze ${expected_version#v}"
run_smoke_test "z3ed --self-test" "$mount_dir" "$mount_dir/z3ed" --self-test

echo "DMG validation passed."
