#!/usr/bin/env bash
# run-061-automated-gates.sh
# Execute 0.6.1 automated release gates and write evidence artifacts.
#
# Usage:
#   bash scripts/dev/run-061-automated-gates.sh \
#     [--build-dir build_ai] \
#     [--qa-dir QA/YYYY-MM-DD-v0.6.1] \
#     [--rom roms/oos168.sfc]

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

BUILD_DIR="build_ai"
ROM_PATH="roms/oos168.sfc"
QA_DIR="QA/$(date +%Y-%m-%d)-v0.6.1"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)
      BUILD_DIR="$2"
      shift 2
      ;;
    --qa-dir)
      QA_DIR="$2"
      shift 2
      ;;
    --rom)
      ROM_PATH="$2"
      shift 2
      ;;
    --help|-h)
      sed -n '1,40p' "$0" | sed 's/^# \{0,1\}//'
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      exit 2
      ;;
  esac
done

LOG_DIR="$QA_DIR/logs"
SUMMARY_FILE="$QA_DIR/automation-summary.org"
mkdir -p "$LOG_DIR" "$QA_DIR/notes"

RESULTS=()

run_gate() {
  local gate_id="$1"
  local name="$2"
  local expected="$3"
  local logfile="$4"
  shift 4

  local cmd=("$@")
  local log_path="$LOG_DIR/$logfile"

  printf '\n[%s] %s\n' "$gate_id" "$name"
  printf 'cmd: %q ' "${cmd[@]}"
  printf '\n'

  set +e
  "${cmd[@]}" >"$log_path" 2>&1
  local rc=$?
  set -e

  local status="FAIL"
  local actual="exit $rc"

  case "$expected" in
    exit0)
      if [[ $rc -eq 0 ]]; then
        status="PASS"
      fi
      ;;
    nonzero)
      if [[ $rc -ne 0 ]]; then
        status="PASS"
      fi
      ;;
    *)
      echo "Internal error: unknown expected policy '$expected'" >&2
      exit 3
      ;;
  esac

  RESULTS+=("$gate_id|$name|$expected|$actual|$status|$log_path")

  if [[ "$status" == "PASS" ]]; then
    printf '[PASS] %s (%s)\n' "$gate_id" "$actual"
  else
    printf '[FAIL] %s (%s)\n' "$gate_id" "$actual"
    tail -n 60 "$log_path" || true
  fi
}

FOCUSED_FILTER='TileObjectHandlerTest.*:TileSelectorWidgetTest.*:SpriteInteractionHandlerTest.*:DoorInteractionHandlerTest.*:DungeonWorkbenchToolbarTest.*:DungeonCanvasViewerNavigationTest.*:DungeonOverlayControlsTest.*:ProjectBundleVerifyTest.*:ProjectBundlePackTest.*:ProjectBundleUnpackTest.*:ProjectBundleArchiveTest.*:OracleSmokeCheckTest.*:DungeonOraclePreflightTest.*'

run_gate A1 "Build yaze_test_unit + z3ed" exit0 "A1-build.log" \
  cmake --build "$BUILD_DIR" --target yaze_test_unit z3ed --parallel 8

run_gate A2 "Focused unit tests" exit0 "A2-focused.log" \
  "$BUILD_DIR/bin/Debug/yaze_test_unit" --gtest_filter="$FOCUSED_FILTER"

run_gate A3 "Quick editor suite" exit0 "A3-quick-editor.log" \
  ctest --test-dir "$BUILD_DIR" -R yaze_test_quick_unit_editor --output-on-failure

if [[ -f "$ROM_PATH" ]]; then
  run_gate A4 "oracle-smoke-check structural" exit0 "A4-structural.log" \
    "$BUILD_DIR/bin/Debug/z3ed" oracle-smoke-check --rom "$ROM_PATH" --min-d6-track-rooms=4 --format=json

  run_gate A5 "oracle-smoke-check strict (expected fail)" nonzero "A5-strict.log" \
    "$BUILD_DIR/bin/Debug/z3ed" oracle-smoke-check --rom "$ROM_PATH" --min-d6-track-rooms=4 --strict-readiness --format=json

  run_gate A6 "dungeon-oracle-preflight required rooms (expected fail)" nonzero "A6-preflight.log" \
    "$BUILD_DIR/bin/Debug/z3ed" dungeon-oracle-preflight --rom "$ROM_PATH" --required-collision-rooms=0x25,0x27,0x32 --format=json
else
  RESULTS+=("A4|oracle-smoke-check structural|exit0|SKIP (missing ROM)|SKIP|$LOG_DIR/A4-structural.log")
  RESULTS+=("A5|oracle-smoke-check strict (expected fail)|nonzero|SKIP (missing ROM)|SKIP|$LOG_DIR/A5-strict.log")
  RESULTS+=("A6|dungeon-oracle-preflight required rooms (expected fail)|nonzero|SKIP (missing ROM)|SKIP|$LOG_DIR/A6-preflight.log")
fi

run_gate A11 "NonBundleZipCleansUpByDefault" exit0 "A11-cleanup-test.log" \
  "$BUILD_DIR/bin/Debug/yaze_test_unit" --gtest_filter='ProjectBundleUnpackTest.NonBundleZipCleansUpByDefault'

run_gate A12 "KeepPartialOutputPreservesFilesOnFailure" exit0 "A12-keep-partial-test.log" \
  "$BUILD_DIR/bin/Debug/yaze_test_unit" --gtest_filter='ProjectBundleUnpackTest.KeepPartialOutputPreservesFilesOnFailure'

branch_name="$(git rev-parse --abbrev-ref HEAD)"
short_sha="$(git rev-parse --short HEAD)"

{
  echo "#+TITLE: 0.6.1 Automated Gate Evidence"
  echo "#+DATE: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo
  echo "* Session"
  echo "- Branch: $branch_name"
  echo "- Commit: $short_sha"
  echo "- Build dir: $BUILD_DIR"
  echo "- ROM: $ROM_PATH"
  echo "- QA dir: $QA_DIR"
  echo
  echo "* Results"
  echo "| ID | Gate | Expected | Actual | Pass/Fail | Log |"
  echo "|----+------+----------+--------+-----------+-----|"
  for row in "${RESULTS[@]}"; do
    IFS='|' read -r id gate expected actual status log <<<"$row"
    echo "| $id | $gate | $expected | $actual | $status | $log |"
  done
} > "$SUMMARY_FILE"

failed=0
for row in "${RESULTS[@]}"; do
  IFS='|' read -r _ _ _ _ status _ <<<"$row"
  if [[ "$status" == "FAIL" ]]; then
    failed=1
    break
  fi
done

echo ""
echo "Summary written to: $SUMMARY_FILE"

if [[ $failed -ne 0 ]]; then
  echo "Automated gate FAILED"
  exit 1
fi

echo "Automated gate PASSED"
exit 0
