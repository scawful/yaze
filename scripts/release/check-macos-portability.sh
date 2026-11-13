#!/bin/bash
# Fails when packaged macOS binaries load a dylib outside the system and the
# app bundle (for example /opt/homebrew), or require a newer macOS than
# --max-minos. Checks yaze.app's executable, its bundled dylibs, and z3ed.
#
# Usage: check-macos-portability.sh <dir containing yaze.app> [--max-minos 15.0]
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <package-root> [--max-minos <version>]"
  exit 2
fi

root="$1"
shift
max_minos=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --max-minos)
      [[ $# -ge 2 ]] || { echo "Missing value for --max-minos"; exit 2; }
      max_minos="$2"
      shift 2
      ;;
    *)
      echo "Unknown argument: $1"
      exit 2
      ;;
  esac
done

binaries=("$root/yaze.app/Contents/MacOS/yaze")
[[ -f "$root/z3ed" ]] && binaries+=("$root/z3ed")
if [[ -d "$root/yaze.app/Contents/Frameworks" ]]; then
  while IFS= read -r -d '' dylib; do
    binaries+=("$dylib")
  done < <(find "$root/yaze.app/Contents/Frameworks" -type f -name '*.dylib' -print0)
fi

# "1.2.3" -> comparable integer.
version_key() {
  local major minor patch
  IFS=. read -r major minor patch <<< "$1"
  echo $(( ${major:-0} * 10000 + ${minor:-0} * 100 + ${patch:-0} ))
}

failed=0
for binary in "${binaries[@]}"; do
  label="${binary#"$root"/}"
  if [[ ! -f "$binary" ]]; then
    echo "Missing binary: $label"
    failed=1
    continue
  fi

  while read -r dep; do
    case "$dep" in
      /usr/lib/* | /System/* | @rpath/* | @loader_path/* | @executable_path/*) ;;
      *)
        echo "$label loads $dep (not system or bundled)"
        failed=1
        ;;
    esac
  done < <(otool -L "$binary" | tail -n +2 | awk '{print $1}')

  minos="$(otool -l "$binary" |
    awk '/cmd LC_BUILD_VERSION/ {found=1} found && $1 == "minos" {print $2; exit}')"
  echo "$label: minos ${minos:-unknown}"
  if [[ -n "$max_minos" ]]; then
    if [[ -z "$minos" ]]; then
      echo "$label has no LC_BUILD_VERSION minos"
      failed=1
    elif [[ $(version_key "$minos") -gt $(version_key "$max_minos") ]]; then
      echo "$label requires macOS $minos, newer than $max_minos"
      failed=1
    fi
  fi
done

exit "$failed"
