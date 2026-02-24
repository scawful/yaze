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

mock_project="$tmp_dir/mock-project"
mkdir -p "$mock_project/scripts/agents"
mock_coord="$mock_project/scripts/agents/coord"
cat >"$mock_coord" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
if [[ "${1:-}" != "task-list" ]]; then
  exit 0
fi
fmt="text"
for ((i = 1; i <= $#; i++)); do
  if [[ "${!i}" == "--format" ]]; then
    j=$((i + 1))
    fmt="${!j:-text}"
    break
  fi
done
if [[ "$fmt" == "json" ]]; then
  python3 - <<'PY'
import json
from datetime import datetime, timedelta, timezone
now = datetime.now(timezone.utc)
payload = [
    {
        "task_id": "task_recent",
        "status": "active",
        "assignee": "ai-infra-architect",
        "priority": "B",
        "title": "Recent task",
        "updated_at": now.isoformat().replace("+00:00", "Z"),
    },
    {
        "task_id": "task_old",
        "status": "active",
        "assignee": "ai-infra-architect",
        "priority": "B",
        "title": "Old imported task",
        "updated_at": (now - timedelta(minutes=240)).isoformat().replace("+00:00", "Z"),
    },
]
print(json.dumps(payload))
PY
  exit 0
fi
cat <<TXT
task_id	status	assignee	priority	title
task_recent	active	ai-infra-architect	B	Recent task
task_old	active	ai-infra-architect	B	Old imported task
TXT
EOF
chmod +x "$mock_coord"

out_coord="$("$WATCH_SCRIPT" \
  --once \
  --project-root "$mock_project" \
  --task-root "$task_root" \
  --window-min 10 \
  --tail 2 \
  --max-files 2 \
  --coord-recent-min 30)"

echo "$out_coord" | grep -q "Active Tasks, <= 30m" || {
  echo "FAIL: coord recency header not shown" >&2
  exit 5
}
echo "$out_coord" | grep -q "task_recent" || {
  echo "FAIL: recent coord task missing from filtered output" >&2
  exit 6
}
echo "$out_coord" | grep -q "task_old" && {
  echo "FAIL: old coord task should be filtered out" >&2
  exit 7
}

echo "PASS: watch-claude-tasks symlink/regular log discovery + coord recency filter"
