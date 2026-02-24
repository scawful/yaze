#!/usr/bin/env bash
# agent-guardrail-loop.sh
#
# Watches Claude task outputs and runs focused guardrail tests whenever new
# output appears. Intended for local multi-agent sessions.
#
# Usage:
#   bash scripts/dev/agent-guardrail-loop.sh
#   INTERVAL_SEC=240 BUILD_DIR=build_ai bash scripts/dev/agent-guardrail-loop.sh

set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build_ai}"
INTERVAL_SEC="${INTERVAL_SEC:-180}"
TASK_ROOT="${TASK_ROOT:-/private/tmp/claude-$(id -u)/-Users-scawful-src-hobby-yaze/tasks}"
STATE_FILE="${STATE_FILE:-/tmp/yaze-agent-guardrail.last}"
LOG_DIR="${LOG_DIR:-/tmp/yaze-agent-guardrail}"
PROJECT_ROOT="${PROJECT_ROOT:-$(pwd)}"

mkdir -p "$LOG_DIR"

timestamp() {
  date +"%Y-%m-%d %H:%M:%S"
}

latest_output_file() {
  python3 - "$TASK_ROOT" <<'PY'
import glob
import os
import sys

root = sys.argv[1]
best_path = ""
best_mtime = -1.0

for path in glob.glob(os.path.join(root, "*.output")):
    if not (os.path.isfile(path) or os.path.islink(path)):
        continue
    mtime = None
    if os.path.islink(path):
        try:
            mtime = os.stat(os.path.realpath(path)).st_mtime
        except FileNotFoundError:
            mtime = os.lstat(path).st_mtime
    else:
        mtime = os.path.getmtime(path)
    if mtime > best_mtime:
        best_mtime = mtime
        best_path = path

if best_path:
    print(best_path)
PY
}

file_signature() {
  local f="$1"
  local sig_file="$f"
  if [[ -L "$f" ]]; then
    local resolved
    resolved="$(python3 - "$f" <<'PY'
import os
import sys
print(os.path.realpath(sys.argv[1]))
PY
)"
    if [[ -f "$resolved" ]]; then
      sig_file="$resolved"
    fi
  fi
  if [ "$(uname -s)" = "Darwin" ]; then
    stat -f "%m:%z:%N" "$sig_file"
  else
    stat -c "%Y:%s:%n" "$sig_file"
  fi
}

run_guardrails() {
  local ts="$1"
  local unit_log="$LOG_DIR/${ts}.unit.log"
  local quick_log="$LOG_DIR/${ts}.quick.log"

  local gfilter
  gfilter="TileSelectorWidgetTest.*"
  gfilter+=":SpriteInteractionHandlerTest.*"
  gfilter+=":DoorInteractionHandlerTest.*"
  gfilter+=":TileObjectHandlerTest.*"
  gfilter+=":ProjectBundlePackTest.*"
  gfilter+=":ProjectBundleUnpackTest.*"
  gfilter+=":ProjectBundleArchiveTest.*"

  echo "[$(timestamp)] [guardrail] running focused gtests..."
  if "$BUILD_DIR/bin/Debug/yaze_test_unit" --gtest_filter="$gfilter" \
      >"$unit_log" 2>&1; then
    echo "[$(timestamp)] [guardrail] focused gtests: PASS"
  else
    echo "[$(timestamp)] [guardrail] focused gtests: FAIL (see $unit_log)"
  fi

  echo "[$(timestamp)] [guardrail] running quick editor ctest..."
  if ctest --test-dir "$BUILD_DIR" -R yaze_test_quick_unit_editor \
      --output-on-failure >"$quick_log" 2>&1; then
    echo "[$(timestamp)] [guardrail] quick editor: PASS"
  else
    echo "[$(timestamp)] [guardrail] quick editor: FAIL (see $quick_log)"
  fi
}

print_active_coord_tasks() {
  if [[ "${SHOW_COORD_TASKS:-1}" != "1" ]]; then
    return
  fi
  local coord="$PROJECT_ROOT/scripts/agents/coord"
  if [[ ! -x "$coord" ]]; then
    return
  fi
  local out
  out="$("$coord" task-list --status active --format text 2>/dev/null || true)"
  if [[ -n "$out" ]]; then
    echo "[$(timestamp)] [coord] active tasks:"
    echo "$out" | sed 's/^/[coord] /'
  fi
}

last_sig=""
if [ -f "$STATE_FILE" ]; then
  last_sig="$(cat "$STATE_FILE" 2>/dev/null || true)"
fi

echo "[$(timestamp)] agent-guardrail-loop started"
echo "[$(timestamp)] task_root=$TASK_ROOT interval=${INTERVAL_SEC}s build_dir=$BUILD_DIR"

while true; do
  latest="$(latest_output_file)"
  if [ -z "$latest" ]; then
    echo "[$(timestamp)] [watch] no task outputs yet"
    sleep "$INTERVAL_SEC"
    continue
  fi

  sig="$(file_signature "$latest")"
  if [ "$sig" != "$last_sig" ]; then
    echo "[$(timestamp)] [watch] update detected: $(basename "$latest")"
    print_active_coord_tasks

    if pgrep -f "cmake --build $BUILD_DIR" >/dev/null 2>&1; then
      echo "[$(timestamp)] [guardrail] build in progress; deferring tests"
    else
      run_guardrails "$(date +%Y%m%d_%H%M%S)"
    fi

    last_sig="$sig"
    printf "%s" "$last_sig" > "$STATE_FILE"
  fi

  sleep "$INTERVAL_SEC"
done
