#!/usr/bin/env bash
# Heal Ninja metadata corruption by rotating .ninja_deps/.ninja_log and rebuilding.

set -euo pipefail

usage() {
  cat <<'EOF'
Usage: scripts/agents/ninja-heal.sh [options]

Options:
  --preset <name>       CMake build preset to use (default: dev)
  --build-dir <path>    Build directory containing .ninja_* files (default: build)
  --parallel, -j <n>    Parallel build jobs (default: YAZE_BUILD_JOBS/CMAKE_BUILD_PARALLEL_LEVEL/4)
  --no-verify           Skip second incremental verification build
  --help, -h            Show this help

Examples:
  scripts/agents/ninja-heal.sh
  scripts/agents/ninja-heal.sh --preset dev --build-dir build --parallel 8
EOF
}

PRESET="dev"
BUILD_DIR="build"
PARALLEL="${YAZE_BUILD_JOBS:-${CMAKE_BUILD_PARALLEL_LEVEL:-4}}"
VERIFY=1

while [[ $# -gt 0 ]]; do
  case "$1" in
    --preset)
      PRESET="$2"
      shift 2
      ;;
    --build-dir)
      BUILD_DIR="$2"
      shift 2
      ;;
    --parallel|-j)
      PARALLEL="$2"
      shift 2
      ;;
    --no-verify)
      VERIFY=0
      shift
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      echo "Unknown argument: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

if [[ ! -d "$BUILD_DIR" ]]; then
  echo "Build directory '$BUILD_DIR' not found; configuring preset '$PRESET' first..."
  cmake --preset "$PRESET"
fi

if [[ ! -f "$BUILD_DIR/build.ninja" ]]; then
  echo "Missing '$BUILD_DIR/build.ninja'; configuring preset '$PRESET'..."
  cmake --preset "$PRESET"
fi

STAMP="$(date +%Y%m%d-%H%M%S)"
for state_file in .ninja_deps .ninja_log; do
  src="$BUILD_DIR/$state_file"
  if [[ -f "$src" ]]; then
    dst="$BUILD_DIR/${state_file}.${STAMP}.bak"
    mv "$src" "$dst"
    echo "Backed up $src -> $dst"
  fi
done

LOG1="/tmp/yaze-ninja-heal-${STAMP}-build1.log"
LOG2="/tmp/yaze-ninja-heal-${STAMP}-build2.log"

echo "Running recovery build (preset=$PRESET, jobs=$PARALLEL)..."
cmake --build --preset "$PRESET" --parallel "$PARALLEL" 2>&1 | tee "$LOG1"

if grep -q "premature end of file" "$LOG1"; then
  echo "Warning: Ninja warning still present during recovery build." >&2
fi

if [[ "$VERIFY" -eq 1 ]]; then
  echo "Running verification build..."
  cmake --build --preset "$PRESET" --parallel "$PARALLEL" 2>&1 | tee "$LOG2"
  if grep -q "premature end of file" "$LOG2"; then
    echo "Verification failed: Ninja warning still present after heal." >&2
    echo "Inspect logs: $LOG1 and $LOG2" >&2
    exit 2
  fi
  if grep -q "no work to do" "$LOG2"; then
    echo "Verification passed: build reached steady state (no work to do)."
  else
    echo "Verification note: incremental build still had some work; this can be normal after deep dependency refresh."
  fi
fi

if [[ "$VERIFY" -eq 1 ]]; then
  echo "Ninja metadata heal complete. Logs: $LOG1, $LOG2"
else
  echo "Ninja metadata heal complete. Logs: $LOG1"
fi