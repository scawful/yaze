#!/usr/bin/env bash
# Quick smoke build for a given preset in an isolated directory with timing info.

set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <preset> [build_dir]" >&2
  exit 1
fi

PRESET="$1"

START=$(date +%s)
cmake --preset "$PRESET"
cmake --build --preset "$PRESET"
END=$(date +%s)

ELAPSED=$((END - START))
echo "Smoke build '$PRESET' completed in ${ELAPSED}s"
