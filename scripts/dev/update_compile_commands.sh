#!/usr/bin/env bash
set -euo pipefail

# Keep repo-root compile_commands.json pointing at a preset build.
#
# Usage:
#   scripts/dev/update_compile_commands.sh mac-ai
#
# Notes:
# - clangd reads the repo root compile_commands.json (see .clangd).
# - This avoids editing .clangd when switching presets.

preset="${1:-mac-ai}"
src="build/presets/${preset}/compile_commands.json"
dst="compile_commands.json"

if [[ ! -f "${src}" ]]; then
  echo "Missing: ${src}" >&2
  echo "Run: cmake --preset ${preset}" >&2
  exit 1
fi

rm -f "${dst}"
ln -s "${src}" "${dst}"
echo "Updated ${dst} -> ${src}"
