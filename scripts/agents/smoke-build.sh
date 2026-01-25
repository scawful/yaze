#!/usr/bin/env bash
# Quick smoke build for a given preset in an isolated directory with timing info.

set -euo pipefail

setup_ccache() {
  if ! command -v ccache >/dev/null 2>&1; then
    return
  fi

  local tmp_root="${TMPDIR:-/tmp}"
  if [[ -z "${CCACHE_DIR:-}" ]]; then
    export CCACHE_DIR="${tmp_root%/}/ccache"
  fi
  if [[ -z "${CCACHE_TEMPDIR:-}" ]]; then
    export CCACHE_TEMPDIR="${tmp_root%/}/ccache/tmp"
  fi
  mkdir -p "$CCACHE_DIR" "$CCACHE_TEMPDIR" || true
}

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <preset> [build_dir]" >&2
  exit 1
fi

PRESET="$1"
BUILD_JOBS="${YAZE_BUILD_JOBS:-${CMAKE_BUILD_PARALLEL_LEVEL:-4}}"

START=$(date +%s)
setup_ccache
cmake --preset "$PRESET"
cmake --build --preset "$PRESET" --parallel "$BUILD_JOBS"
END=$(date +%s)

ELAPSED=$((END - START))
echo "Smoke build '$PRESET' completed in ${ELAPSED}s"
