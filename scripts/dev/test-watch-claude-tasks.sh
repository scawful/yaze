#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
WATCH_SCRIPT="$PROJECT_ROOT/scripts/dev/watch-claude-tasks.sh"

if [[ ! -x "$WATCH_SCRIPT" ]]; then
  echo "error: missing executable watcher: $WATCH_SCRIPT" >&2
  exit 1
fi

tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

task_root="$tmp_dir/tasks"
mkdir -p "$task_root"

real_log="$tmp_dir/agent-live.jsonl"
cat >"$real_log" <<'EOF'
{"type":"assistant","message":{"content":[{"type":"text","text":"live-update"}]}}
EOF
ln -s "$real_log" "$task_root/agent-a.output"

regular_log="$task_root/agent-b.output"
cat >"$regular_log" <<'EOF'
{"type":"assistant","message":{"content":[{"type":"text","text":"regular-file"}]}}
EOF

out="$("$WATCH_SCRIPT" --once --task-root "$task_root" --window-min 10 --tail 5 --max-files 4)"

echo "$out" | grep -q "agent-a.output" || {
  echo "FAIL: symlink-backed output was not discovered" >&2
  exit 2
}
echo "$out" | grep -q "live-update" || {
  echo "FAIL: symlink-backed output content was not tailed" >&2
  exit 3
}
echo "$out" | grep -q "agent-b.output" || {
  echo "FAIL: regular output file was not discovered" >&2
  exit 4
}

echo "PASS: watch-claude-tasks symlink/regular log discovery"
