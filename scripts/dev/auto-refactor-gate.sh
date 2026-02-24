#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="${PROJECT_ROOT:-$(pwd)}"
BUILD_DIR="${BUILD_DIR:-build_ai}"
TASK_ROOT=""
LOG_DIR="${LOG_DIR:-QA/auto-refactor-gate}"
INTERVAL_SECONDS="${INTERVAL_SECONDS:-20}"
RUN_ONCE=0
SKIP_BUILD=0

GTEST_FILTER_DEFAULT='EditorManagerTest.*:EditorManagerRomWritePolicyTest.*:EditorManagerOracleRomSafetyTest.*:RomFileManagerTest.*:ProjectManagerTest.*:ProjectManagerValidatorTest.*:DungeonEditorV2RomSafetyTest.*:OracleSmokeCheckTest.*:DungeonOraclePreflightTest.*:ProjectBundleVerifyTest.*:ProjectBundlePackTest.*:ProjectBundleUnpackTest.*:ProjectBundleArchiveTest.*:ProjectBundlePathTest.*:ProjectBundleTest.*'
GTEST_FILTER="${GTEST_FILTER:-$GTEST_FILTER_DEFAULT}"

usage() {
  cat <<USAGE
Usage: auto-refactor-gate.sh [options]

Auto-runs refactor regression gate when Claude task logs change.

Options:
  --project-root <path>   Project root (default: cwd)
  --task-root <path>      Claude task spool path (default: derived from /private/tmp)
  --build-dir <path>      CMake build dir (default: build_ai)
  --log-dir <path>        Output log dir (default: QA/auto-refactor-gate)
  --interval <seconds>    Poll interval (default: 20)
  --skip-build            Skip incremental cmake --build step
  --once                  Run gate once and exit (no watching)
  --help                  Show this help

Environment:
  GTEST_FILTER            Override the default 121-test refactor gate filter
USAGE
}

sanitize_path() {
  local path="$1"
  echo "$path" | sed 's#/#-#g'
}

derive_task_root() {
  local uid
  uid="$(id -u)"
  local sanitized
  sanitized="$(sanitize_path "$PROJECT_ROOT")"
  echo "/private/tmp/claude-${uid}/${sanitized}/tasks"
}

latest_log_signature() {
  local root="$1"
  python3 - "$root" <<'PY'
import glob
import os
import sys

root = sys.argv[1]
paths = glob.glob(os.path.join(root, "*.output"))
best = None
for p in paths:
    if not (os.path.isfile(p) or os.path.islink(p)):
        continue
    mtime = None
    target = p
    try:
        if os.path.islink(p):
            target = os.path.realpath(p)
        st = os.stat(target)
        mtime = st.st_mtime
    except FileNotFoundError:
        try:
            st = os.lstat(p)
            mtime = st.st_mtime
        except FileNotFoundError:
            continue
    if best is None or mtime > best[0]:
        best = (mtime, p)

if best is None:
    print("")
else:
    print(f"{best[0]:.6f}|{best[1]}")
PY
}

run_gate() {
  local trigger_sig="$1"
  local ts
  ts="$(date '+%Y%m%d-%H%M%S')"
  mkdir -p "$LOG_DIR"
  local log_file="$LOG_DIR/${ts}-refactor-gate.log"
  local status_file="$LOG_DIR/latest.status"

  {
    echo "=== auto-refactor-gate ==="
    echo "timestamp: $(date '+%Y-%m-%d %H:%M:%S')"
    echo "project_root: $PROJECT_ROOT"
    echo "build_dir: $BUILD_DIR"
    echo "trigger: $trigger_sig"
    echo "gtest_filter: $GTEST_FILTER"
    echo

    if [[ "$SKIP_BUILD" -eq 0 ]]; then
      echo "[1/2] Incremental build: yaze_test_unit"
      cmake --build "$BUILD_DIR" --target yaze_test_unit --parallel 8
      echo
    else
      echo "[1/2] Incremental build skipped (--skip-build)"
      echo
    fi

    echo "[2/2] Refactor gate tests"
    "./$BUILD_DIR/bin/Debug/yaze_test_unit" --gtest_filter="$GTEST_FILTER"
  } >"$log_file" 2>&1
  local rc=$?

  {
    echo "timestamp=$(date -u '+%Y-%m-%dT%H:%M:%SZ')"
    echo "exit_code=$rc"
    echo "log_file=$log_file"
    echo "trigger=$trigger_sig"
  } > "$status_file"

  if [[ $rc -eq 0 ]]; then
    echo "[PASS] refactor gate: $log_file"
  else
    echo "[FAIL] refactor gate: $log_file"
  fi
  return $rc
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --project-root)
      PROJECT_ROOT="${2:-}"
      shift 2
      ;;
    --task-root)
      TASK_ROOT="${2:-}"
      shift 2
      ;;
    --build-dir)
      BUILD_DIR="${2:-}"
      shift 2
      ;;
    --log-dir)
      LOG_DIR="${2:-}"
      shift 2
      ;;
    --interval)
      INTERVAL_SECONDS="${2:-}"
      shift 2
      ;;
    --skip-build)
      SKIP_BUILD=1
      shift
      ;;
    --once)
      RUN_ONCE=1
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

if [[ -z "$TASK_ROOT" ]]; then
  TASK_ROOT="$(derive_task_root)"
fi

cd "$PROJECT_ROOT"

if [[ "$RUN_ONCE" -eq 1 ]]; then
  run_gate "manual-once"
  exit $?
fi

echo "Watching Claude logs for refactor drops..."
echo "project_root=$PROJECT_ROOT"
echo "task_root=$TASK_ROOT"
echo "build_dir=$BUILD_DIR"
echo "log_dir=$LOG_DIR"
echo

last_sig=""
while true; do
  if [[ -d "$TASK_ROOT" ]]; then
    sig="$(latest_log_signature "$TASK_ROOT")"
    if [[ -n "$sig" && "$sig" != "$last_sig" ]]; then
      echo "[trigger] detected new/updated Claude log: $sig"
      if run_gate "$sig"; then
        last_sig="$sig"
      else
        # Keep last_sig updated to avoid rapid failure loop on same trigger.
        last_sig="$sig"
      fi
    fi
  fi
  sleep "$INTERVAL_SECONDS"
done
