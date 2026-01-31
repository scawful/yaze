#!/usr/bin/env bash
# Build yaze (optional), then run ZScreamCLI validate-yaze against a golden ROM.
# Usage:
#   ./scripts/validate-yaze.sh --yaze-rom=<path> --golden-rom=<path> [--feature=dungeon] [--build-yaze]
#
# Requires ZScreamCLI as a sibling repo (e.g. ~/src/hobby/ZScreamCLI) or set ZSCREAM_CLI_DIR.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
YAZE_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

YAZE_ROM=""
GOLDEN_ROM=""
FEATURE="dungeon"
BUILD_YAZE=false
EXTRA_ARGS=()

while [[ $# -gt 0 ]]; do
  case $1 in
    --yaze-rom=*)
      YAZE_ROM="${1#--yaze-rom=}"
      shift
      ;;
    --yaze-rom)
      YAZE_ROM="$2"
      shift 2
      ;;
    --golden-rom=*)
      GOLDEN_ROM="${1#--golden-rom=}"
      shift
      ;;
    --golden-rom)
      GOLDEN_ROM="$2"
      shift 2
      ;;
    --feature=*)
      FEATURE="${1#--feature=}"
      shift
      ;;
    --feature)
      FEATURE="$2"
      shift 2
      ;;
    --build-yaze)
      BUILD_YAZE=true
      shift
      ;;
    *)
      EXTRA_ARGS+=("$1")
      shift
      ;;
  esac
done

if [[ -z "$YAZE_ROM" ]] || [[ -z "$GOLDEN_ROM" ]]; then
  echo "Usage: $0 --yaze-rom=<path> --golden-rom=<path> [--feature=dungeon] [--build-yaze] [--output=...] [--json]"
  echo ""
  echo "  --yaze-rom    Path to yaze output ROM (e.g. after File > Save ROM)"
  echo "  --golden-rom  Path to ZScream golden ROM (e.g. ZScreamCLI/tests/fixtures/golden/*.sfc)"
  echo "  --feature     Feature to validate: dungeon, overworld, graphics, collision, torches, pits, blocks, entrances, all (default: dungeon)"
  echo "  --build-yaze  Build yaze first (cmake --preset mac-ai && cmake --build build_ai)"
  echo ""
  echo "Requires ZScreamCLI: set ZSCREAM_CLI_DIR or use sibling repo ../ZScreamCLI"
  exit 1
fi

if [[ "$BUILD_YAZE" = true ]]; then
  echo "Building yaze..."
  cd "$YAZE_ROOT"
  if [[ -f CMakePresets.json ]] && grep -q "mac-ai" CMakePresets.json 2>/dev/null; then
    cmake --preset mac-ai
    cmake --build build_ai --preset mac-ai -j8
  else
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j8
  fi
  echo ""
fi

ZSCREAM_CLI_DIR="${ZSCREAM_CLI_DIR:-$YAZE_ROOT/../ZScreamCLI}"
if [[ ! -d "$ZSCREAM_CLI_DIR" ]] || [[ ! -f "$ZSCREAM_CLI_DIR/src/ZScreamCLI/ZScreamCLI.csproj" ]]; then
  echo "Error: ZScreamCLI not found at $ZSCREAM_CLI_DIR"
  echo "  Clone ZScreamCLI as a sibling of yaze, or set ZSCREAM_CLI_DIR"
  exit 1
fi

VALIDATE_SCRIPT="$ZSCREAM_CLI_DIR/scripts/validate-yaze-rom.sh"
if [[ -x "$VALIDATE_SCRIPT" ]]; then
  exec "$VALIDATE_SCRIPT" \
    --yaze-rom="$YAZE_ROM" \
    --golden-rom="$GOLDEN_ROM" \
    --feature="$FEATURE" \
    "${EXTRA_ARGS[@]}"
else
  cd "$ZSCREAM_CLI_DIR"
  exec dotnet run --project src/ZScreamCLI -- validate-yaze \
    --yaze-rom="$YAZE_ROM" \
    --golden-rom="$GOLDEN_ROM" \
    --feature="$FEATURE" \
    "${EXTRA_ARGS[@]}"
fi
