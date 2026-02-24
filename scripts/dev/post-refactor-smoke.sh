#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="${PROJECT_ROOT:-$(pwd)}"
BUILD_DIR="${BUILD_DIR:-build_ai}"
ROM_PATH="${ROM_PATH:-roms/oos168.sfc}"
QA_DIR="${QA_DIR:-QA/$(date +%Y-%m-%d)-post-refactor-smoke}"
LAUNCH_APP=0

GTEST_FILTER='EditorManagerTest.*:EditorManagerRomWritePolicyTest.*:EditorManagerOracleRomSafetyTest.*:RomFileManagerTest.*:ProjectManagerTest.*:ProjectManagerValidatorTest.*:DungeonEditorV2RomSafetyTest.*:OracleSmokeCheckTest.*:DungeonOraclePreflightTest.*:ProjectBundleVerifyTest.*:ProjectBundlePackTest.*:ProjectBundleUnpackTest.*:ProjectBundleArchiveTest.*:ProjectBundlePathTest.*:ProjectBundleTest.*'

usage() {
  cat <<USAGE
Usage: post-refactor-smoke.sh [options]

Runs automated post-refactor smoke checks, writes QA logs, and prints manual app test steps.

Options:
  --project-root <path>   Project root (default: cwd)
  --build-dir <path>      Build dir (default: build_ai)
  --rom <path>            ROM for z3ed smoke check (default: roms/oos168.sfc)
  --qa-dir <path>         QA output dir (default: QA/YYYY-MM-DD-post-refactor-smoke)
  --launch-app            Launch yaze.app after automated checks
  --help                  Show this help
USAGE
}

run_step() {
  local id="$1"
  local label="$2"
  shift 2
  local log="$QA_DIR/logs/${id}.log"
  echo "[$id] $label"
  if "$@" >"$log" 2>&1; then
    echo "  PASS  -> $log"
    echo "$id|$label|PASS|$log" >> "$QA_DIR/summary.txt"
    return 0
  else
    echo "  FAIL  -> $log"
    echo "$id|$label|FAIL|$log" >> "$QA_DIR/summary.txt"
    return 1
  fi
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --project-root)
      PROJECT_ROOT="${2:-}"
      shift 2
      ;;
    --build-dir)
      BUILD_DIR="${2:-}"
      shift 2
      ;;
    --rom)
      ROM_PATH="${2:-}"
      shift 2
      ;;
    --qa-dir)
      QA_DIR="${2:-}"
      shift 2
      ;;
    --launch-app)
      LAUNCH_APP=1
      shift
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      echo "Unknown arg: $1" >&2
      usage
      exit 2
      ;;
  esac
done

cd "$PROJECT_ROOT"
mkdir -p "$QA_DIR/logs"
: > "$QA_DIR/summary.txt"

ok=1
run_step A1 "Build yaze_test_unit + z3ed + yaze" \
  cmake --build "$BUILD_DIR" --target yaze_test_unit z3ed yaze --parallel 8 || ok=0

run_step A2 "Refactor gate test suite (121 tests)" \
  "./$BUILD_DIR/bin/Debug/yaze_test_unit" --gtest_filter="$GTEST_FILTER" || ok=0

if [[ -f "$ROM_PATH" ]]; then
  run_step A3 "oracle-smoke-check structural" \
    "./$BUILD_DIR/bin/Debug/z3ed" oracle-smoke-check --rom "$ROM_PATH" --min-d6-track-rooms=4 --format=json || ok=0
else
  echo "A3|oracle-smoke-check structural|SKIP (missing ROM: $ROM_PATH)|-" >> "$QA_DIR/summary.txt"
  echo "[A3] SKIP (missing ROM: $ROM_PATH)"
fi

cat > "$QA_DIR/manual-steps.txt" <<MANUAL
Post-Refactor Manual App Smoke

1) Launch app:
   ./$BUILD_DIR/bin/Debug/yaze.app/Contents/MacOS/yaze

2) Core manual checks:
   - Open Oracle ROM/project.
   - Dungeon Workbench opens, room navigation works.
   - Place tile object/sprite/door and verify toast + limits behavior.
   - Tile selector: hover preview + range filter + jump-to-ID.
   - Oracle Validation panel: run smoke/preflight and inspect JSON output view.

3) CLI sanity (optional while app open):
   ./$BUILD_DIR/bin/Debug/z3ed oracle-smoke-check --rom "$ROM_PATH" --min-d6-track-rooms=4 --format=json
   ./$BUILD_DIR/bin/Debug/z3ed dungeon-oracle-preflight --rom "$ROM_PATH" --format=json

4) Record findings in:
   docs/internal/agents/oracle-test-evidence-template.org
MANUAL

if [[ "$LAUNCH_APP" -eq 1 ]]; then
  echo "Launching yaze.app..."
  open "./$BUILD_DIR/bin/Debug/yaze.app"
fi

echo
echo "Smoke summary:"
cat "$QA_DIR/summary.txt"
echo
echo "Manual steps: $QA_DIR/manual-steps.txt"

if [[ "$ok" -eq 1 ]]; then
  exit 0
fi
exit 1
