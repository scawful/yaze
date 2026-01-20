#!/usr/bin/env bash
# Quick smoke build for a given preset in an isolated directory with timing info.

set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <preset> [build_dir]" >&2
  exit 1
fi

PRESET="$1"
BUILD_JOBS="${YAZE_BUILD_JOBS:-${CMAKE_BUILD_PARALLEL_LEVEL:-4}}"

START=$(date +%s)
cmake --preset "$PRESET"
cmake --build --preset "$PRESET" --parallel "$BUILD_JOBS"
END=$(date +%s)

ELAPSED=$((END - START))
echo "Smoke build '$PRESET' completed in ${ELAPSED}s"
