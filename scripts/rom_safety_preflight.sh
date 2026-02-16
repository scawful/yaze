#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <rom.sfc> [--out <dir>] [--verify <sha256>] [--mesen <socket_path>]" >&2
  exit 2
fi

ROM_PATH="$1"
shift || true

OUT_DIR="/tmp"
VERIFY_HASH=""
MESEN_SOCKET=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --out)
      OUT_DIR="${2:-}"
      shift 2
      ;;
    --verify)
      VERIFY_HASH="${2:-}"
      if [[ -z "$VERIFY_HASH" ]]; then
        echo "Error: --verify requires a SHA-256 hash argument" >&2
        exit 2
      fi
      shift 2
      ;;
    --mesen)
      MESEN_SOCKET="${2:-}"
      if [[ -z "$MESEN_SOCKET" ]]; then
        echo "Error: --mesen requires a socket path argument" >&2
        exit 2
      fi
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

# Set Mesen2 socket path if provided.
if [[ -n "$MESEN_SOCKET" ]]; then
  export MESEN2_SOCKET_PATH="$MESEN_SOCKET"
  echo "Mesen2 socket target: $MESEN2_SOCKET_PATH"
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

# Platform-aware SHA-256 computation.
compute_sha256() {
  local file="$1"
  if command -v shasum >/dev/null 2>&1; then
    shasum -a 256 "$file" | awk '{print $1}'
  elif command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$file" | awk '{print $1}'
  else
    echo "Error: neither shasum nor sha256sum found" >&2
    return 1
  fi
}

echo
echo "sha256:"
ROM_HASH="$(compute_sha256 "$ROM_PATH")"
echo "$ROM_HASH  $ROM_PATH"

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

# Post-edit hash verification.
if [[ -n "$VERIFY_HASH" ]]; then
  echo
  echo "verify:"
  if [[ "$ROM_HASH" == "$VERIFY_HASH" ]]; then
    echo "SHA-256 matches expected hash"
  else
    echo "Error: SHA-256 mismatch" >&2
    echo "  expected: $VERIFY_HASH" >&2
    echo "  actual:   $ROM_HASH" >&2
    exit 2
  fi
fi

echo
cp -f "$ROM_PATH" "$COPY_PATH"
echo "OK"

