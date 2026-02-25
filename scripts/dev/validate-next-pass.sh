#!/usr/bin/env bash
# validate-next-pass.sh — local validation gate for dungeon/overworld UX passes
#
# Runs focused build + test checks relevant to tile/sprite/door/workbench/widget
# changes. Designed for macOS/Linux dev loops.
#
# Usage: bash scripts/dev/validate-next-pass.sh [--build-dir BUILD_DIR]

set -euo pipefail

BUILD_DIR="${1:-build_ai}"
# Support --build-dir flag
if [ "${1:-}" = "--build-dir" ] && [ -n "${2:-}" ]; then
  BUILD_DIR="$2"
fi

PASS_COUNT=0
FAIL_COUNT=0
SECTION_RESULTS=()

section() {
  echo ""
  echo "================================================================"
  echo "  $1"
  echo "================================================================"
}

record_result() {
  local name="$1"
  local rc="$2"
  if [ "$rc" -eq 0 ]; then
    SECTION_RESULTS+=("PASS  $name")
    PASS_COUNT=$((PASS_COUNT + 1))
  else
    SECTION_RESULTS+=("FAIL  $name")
    FAIL_COUNT=$((FAIL_COUNT + 1))
  fi
}

# --------------------------------------------------------------------------
# 1. Build
# --------------------------------------------------------------------------
section "Build: yaze_test_unit + z3ed"
if cmake --build "$BUILD_DIR" --target yaze_test_unit z3ed --parallel 8; then
  record_result "Build" 0
else
  record_result "Build" 1
  echo "FATAL: Build failed. Stopping."
  exit 1
fi

# --------------------------------------------------------------------------
# 2. Focused gtest: tile/sprite/door/workbench/widget suites
# --------------------------------------------------------------------------
GTEST_FILTER="TileObjectHandlerTest.*"
GTEST_FILTER+=":TileSelectorWidgetTest.*"
GTEST_FILTER+=":SpriteInteractionHandlerTest.*"
GTEST_FILTER+=":DoorInteractionHandlerTest.*"
GTEST_FILTER+=":DungeonWorkbenchToolbarTest.*"
GTEST_FILTER+=":DungeonCanvasViewerNavigationTest.*"
GTEST_FILTER+=":DungeonOverlayControlsTest.*"
GTEST_FILTER+=":ObjectTileEditorTest.*"
GTEST_FILTER+=":ObjectTileLayoutTest.*"

section "Focused gtest: tile/sprite/door/workbench/widget"
if "$BUILD_DIR/bin/Debug/yaze_test_unit" --gtest_filter="$GTEST_FILTER" 2>&1; then
  record_result "Focused gtest" 0
else
  record_result "Focused gtest" 1
fi

# --------------------------------------------------------------------------
# 3. Quick unit editor suite
# --------------------------------------------------------------------------
section "Quick unit editor suite"
EDITOR_BIN="$BUILD_DIR/bin/Debug/yaze_test_quick_unit_editor"
if [ -x "$EDITOR_BIN" ]; then
  if "$EDITOR_BIN" 2>&1; then
    record_result "Quick editor" 0
  else
    record_result "Quick editor" 1
  fi
else
  echo "SKIP: $EDITOR_BIN not found (build target: yaze_test_quick_unit_editor)"
  record_result "Quick editor (skip)" 0
fi

# --------------------------------------------------------------------------
# 4. Oracle smoke check (sanity — no ROM needed for structural pass)
# --------------------------------------------------------------------------
section "Oracle smoke check"
Z3ED="$BUILD_DIR/bin/Debug/z3ed"
if [ -x "$Z3ED" ] && [ -f "roms/oos168.sfc" ]; then
  if "$Z3ED" oracle-smoke-check --rom roms/oos168.sfc --min-d6-track-rooms=4 --format=json > /dev/null 2>&1; then
    record_result "Oracle smoke" 0
  else
    record_result "Oracle smoke" 1
  fi
else
  echo "SKIP: z3ed or ROM not available"
  record_result "Oracle smoke (skip)" 0
fi

# --------------------------------------------------------------------------
# Summary
# --------------------------------------------------------------------------
section "Summary"
for r in "${SECTION_RESULTS[@]}"; do
  echo "  $r"
done
echo ""
TOTAL=$((PASS_COUNT + FAIL_COUNT))
echo "  $PASS_COUNT/$TOTAL passed"

if [ "$FAIL_COUNT" -gt 0 ]; then
  echo ""
  echo "  VALIDATION FAILED"
  exit 1
else
  echo ""
  echo "  ALL CHECKS PASSED"
  exit 0
fi
