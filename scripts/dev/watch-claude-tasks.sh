#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="${PROJECT_ROOT:-$(pwd)}"
TASK_ROOT=""
INTERVAL_SECONDS=8
TAIL_LINES=20
WINDOW_MIN=30
MAX_FILES=8
RUN_ONCE=0

usage() {
  cat <<'EOF'
Usage: watch-claude-tasks.sh [options]

Options:
  --project-root <path>   Project root used to derive Claude task spool path
  --task-root <path>      Explicit task spool path (overrides derived path)
  --interval <seconds>    Refresh interval in watch mode (default: 8)
  --window-min <minutes>  Show logs modified within this many minutes (default: 30)
  --tail <lines>          Tail this many lines per log (default: 20)
  --max-files <count>     Max recent logs to print (default: 8)
  --once                  Print one snapshot and exit
  -h, --help              Show this help
EOF
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

format_stat() {
  local file="$1"
  if stat -f "%Sm|%z" -t "%Y-%m-%d %H:%M:%S" "$file" >/dev/null 2>&1; then
    stat -f "%Sm|%z" -t "%Y-%m-%d %H:%M:%S" "$file"
  else
    stat -c "%y|%s" "$file"
  fi
}

collect_recent_logs() {
  local root="$1"
  python3 - "$root" "$WINDOW_MIN" "$MAX_FILES" <<'PY'
import glob
import os
import sys
import time

root = sys.argv[1]
window_seconds = float(sys.argv[2]) * 60.0
max_files = int(sys.argv[3])
now = time.time()
rows = []

for path in glob.glob(os.path.join(root, "*.output")):
    if not (os.path.isfile(path) or os.path.islink(path)):
        continue

    display_path = path
    content_path = path
    try:
        st = os.lstat(path)
    except FileNotFoundError:
        continue

    mtime = st.st_mtime
    size = st.st_size

    if os.path.islink(path):
        resolved = os.path.realpath(path)
        try:
            rst = os.stat(resolved)
            content_path = resolved
            mtime = rst.st_mtime
            size = rst.st_size
        except FileNotFoundError:
            # Broken symlink: keep link metadata for visibility.
            content_path = path

    if now - mtime <= window_seconds:
        rows.append((mtime, display_path, content_path, size))

rows.sort(key=lambda r: r[0], reverse=True)
for mtime, display_path, content_path, size in rows[:max_files]:
    print(f"{mtime}\t{display_path}\t{content_path}\t{size}")
PY
}

print_claude_processes() {
  echo "== Active Claude Sessions =="
  local proc
  proc="$(ps -Ao pid,ppid,%cpu,etime,command | \
    awk '/\/\.local\/bin\/claude --dangerously-skip-permissions/ {printf "  pid=%s ppid=%s cpu=%s etime=%s  %s\n", $1, $2, $3, $4, substr($0, index($0, $5))}')"
  if [[ -z "$proc" ]]; then
    echo "  (none found)"
    return
  fi
  echo "$proc"
  echo
  echo "== Active Child Shell Jobs =="
  local pids
  pids="$(echo "$proc" | awk -F'pid=| ppid=' '{print $2}' | awk '{print $1}')"
  local found=0
  while read -r pid; do
    [[ -z "$pid" ]] && continue
    local children
    children="$(ps -Ao pid,ppid,%cpu,etime,command | awk -v p="$pid" '$2==p')"
    if [[ -n "$children" ]]; then
      found=1
      echo "$children" | sed 's/^/  /'
    fi
  done <<< "$pids"
  if [[ "$found" -eq 0 ]]; then
    echo "  (no direct child shell jobs visible)"
  fi
}

print_recent_logs() {
  local root="$1"
  echo
  echo "== Recent Claude Task Logs (${WINDOW_MIN}m window) =="
  if [[ ! -d "$root" ]]; then
    echo "  task root not found: $root"
    return
  fi

  local records
  records="$(collect_recent_logs "$root")"
  if [[ -z "$records" ]]; then
    echo "  (no recent log files)"
    return
  fi

  while IFS=$'\t' read -r _epoch display_file content_file reported_size; do
    [[ -z "$display_file" ]] && continue
    local meta
    meta="$(format_stat "$content_file")"
    local mtime size
    mtime="${meta%%|*}"
    size="${reported_size:-${meta##*|}}"
    echo "-- $(basename "$display_file")  size=${size}B  mtime=${mtime}"
    if [[ "$size" -eq 0 ]]; then
      echo "   (empty log so far)"
    else
      tail -n "$TAIL_LINES" "$content_file" | sed 's/^/   /'
    fi
    echo
  done <<< "$records"
}

print_coord_tasks() {
  if [[ "${SHOW_COORD_TASKS:-1}" != "1" ]]; then
    return
  fi
  local coord="$PROJECT_ROOT/scripts/agents/coord"
  if [[ ! -x "$coord" ]]; then
    return
  fi
  echo
  echo "== Universe Coordination (Active Tasks) =="
  local out
  if out="$("$coord" task-list --status active --format text 2>/dev/null)"; then
    if [[ -n "$out" ]]; then
      echo "$out" | sed 's/^/  /'
    else
      echo "  (none)"
    fi
  else
    echo "  (unavailable)"
  fi
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
    --interval)
      INTERVAL_SECONDS="${2:-}"
      shift 2
      ;;
    --window-min)
      WINDOW_MIN="${2:-}"
      shift 2
      ;;
    --tail)
      TAIL_LINES="${2:-}"
      shift 2
      ;;
    --max-files)
      MAX_FILES="${2:-}"
      shift 2
      ;;
    --once)
      RUN_ONCE=1
      shift
      ;;
    -h|--help)
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

run_snapshot() {
  echo
  echo "============================================================"
  echo "Claude monitor @ $(date '+%Y-%m-%d %H:%M:%S')"
  echo "project_root=$PROJECT_ROOT"
  echo "task_root=$TASK_ROOT"
  echo "============================================================"
  print_claude_processes
  print_coord_tasks
  print_recent_logs "$TASK_ROOT"
}

if [[ "$RUN_ONCE" -eq 1 ]]; then
  run_snapshot
  exit 0
fi

while true; do
  run_snapshot
  sleep "$INTERVAL_SECONDS"
done
