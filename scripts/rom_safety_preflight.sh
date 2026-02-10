#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <rom.sfc> [--out <dir>]" >&2
  exit 2
fi

ROM_PATH="$1"
shift || true

OUT_DIR="/tmp"
while [[ $# -gt 0 ]]; do
  case "$1" in
    --out)
      OUT_DIR="${2:-}"
      shift 2
      ;;
    *)
      echo "Unknown arg: $1" >&2
      exit 2
      ;;
  esac
done

if [[ ! -f "$ROM_PATH" ]]; then
  echo "ROM not found: $ROM_PATH" >&2
  exit 1
fi

mkdir -p "$OUT_DIR"

BASE="$(basename "$ROM_PATH")"
STEM="${BASE%.*}"
EXT="${BASE##*.}"
if [[ "$EXT" == "$BASE" ]]; then
  EXT="sfc"
fi

TS="$(date +"%Y%m%d-%H%M%S")"
COPY_PATH="${OUT_DIR}/${STEM}-work-${TS}.${EXT}"

echo "ROM:  $ROM_PATH"
echo "Copy: $COPY_PATH"

echo
echo "sha256:"
shasum -a 256 "$ROM_PATH"

echo
echo "size:"
wc -c "$ROM_PATH" | awk '{print $1 " bytes"}'

echo
if command -v z3ed >/dev/null 2>&1; then
  echo "z3ed rom-doctor (json):"
  # Best-effort: don't fail the preflight if rom-doctor errors.
  z3ed rom-doctor --rom "$ROM_PATH" --format json || true
else
  echo "z3ed not found in PATH (skipping rom-doctor)"
fi

echo
cp -f "$ROM_PATH" "$COPY_PATH"
echo "OK"

