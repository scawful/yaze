#!/usr/bin/env bash
# coord-heartbeat.sh
#
# Convenience helper around scripts/agents/coord task-heartbeat.
# - If --id is provided, heartbeats that task.
# - Otherwise, selects the most recent active task for the project,
#   preferring tasks assigned to --agent.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
COORD_BIN="$SCRIPT_DIR/coord"

if [[ ! -x "$COORD_BIN" ]]; then
  echo "error: missing executable: $COORD_BIN" >&2
  exit 1
fi

usage() {
  cat <<'EOF'
Usage: coord-heartbeat.sh --agent <agent-id> [options]

Options:
  --agent <id>         Required agent id (used for heartbeat event).
  --id <task-id>       Heartbeat a specific task id.
  --project-key <key>  Project key override (defaults to coord wrapper behavior).
  --note <text>        Optional heartbeat note.
  --recent-min <m>     Only consider active tasks updated within m minutes when auto-selecting (0 = all).
  --dry-run            Print selected task and heartbeat command without writing event.
  -h, --help           Show help.

Examples:
  scripts/agents/coord-heartbeat.sh --agent ai-infra-architect --note "still working"
  scripts/agents/coord-heartbeat.sh --agent ai-infra-architect --id task_20260224T010203Z_12345
  scripts/agents/coord-heartbeat.sh --agent ai-infra-architect --recent-min 180 --dry-run
EOF
}

agent=""
task_id=""
project_key=""
note=""
recent_min="0"
dry_run=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --agent) agent="${2:-}"; shift 2 ;;
    --id) task_id="${2:-}"; shift 2 ;;
    --project-key) project_key="${2:-}"; shift 2 ;;
    --note) note="${2:-}"; shift 2 ;;
    --recent-min) recent_min="${2:-}"; shift 2 ;;
    --dry-run) dry_run=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *)
      echo "error: unknown arg: $1" >&2
      usage
      exit 2
      ;;
  esac
done

[[ -n "$agent" ]] || {
  echo "error: --agent is required" >&2
  usage
  exit 2
}

if [[ -z "$task_id" ]]; then
  list_args=(task-list --status active --format json)
  if [[ -n "$project_key" ]]; then
    list_args+=(--project-key "$project_key")
  fi

  task_json="$("$COORD_BIN" "${list_args[@]}")"

  selected="$(
    COORD_TASK_JSON="$task_json" python3 - "$agent" "$recent_min" <<'PY'
import json
import os
import sys
import time
from datetime import datetime

agent = sys.argv[1]
recent_min = float(sys.argv[2])
raw = os.environ.get("COORD_TASK_JSON", "")

if not raw:
    raise SystemExit(10)

try:
    payload = json.loads(raw)
except json.JSONDecodeError:
    raise SystemExit(11)

tasks = payload.get("tasks", payload) if isinstance(payload, dict) else payload
if not isinstance(tasks, list):
    raise SystemExit(12)

def parse_ts(value: str):
    if not value:
        return None
    try:
        return datetime.fromisoformat(value.replace("Z", "+00:00")).timestamp()
    except ValueError:
        return None

now = time.time()
rows = []
for task in tasks:
    if not isinstance(task, dict):
        continue
    if task.get("status") != "active":
        continue
    updated_ts = parse_ts(str(task.get("updated_at", "")))
    if updated_ts is None:
        continue
    age_min = (now - updated_ts) / 60.0
    if recent_min > 0 and age_min > recent_min:
        continue
    rows.append((updated_ts, age_min, task))

if not rows:
    raise SystemExit(20)

rows.sort(key=lambda item: item[0], reverse=True)
preferred = [row for row in rows if str(row[2].get("assignee", "")) == agent]
chosen = preferred[0] if preferred else rows[0]

task = chosen[2]
print(
    "\t".join(
        [
            str(task.get("task_id", "")),
            str(task.get("assignee", "")),
            str(task.get("title", "")),
            f"{chosen[1]:.1f}",
            "agent-preferred" if preferred else "latest-active",
        ]
    )
)
PY
  )" || {
    code=$?
    if [[ "$code" -eq 20 ]]; then
      echo "error: no active task candidates found (check --recent-min / --project-key)" >&2
    else
      echo "error: failed to auto-select task (code=$code)" >&2
    fi
    exit "$code"
  }

  IFS=$'\t' read -r task_id selected_assignee selected_title selected_age selected_mode <<< "$selected"
  [[ -n "$task_id" ]] || {
    echo "error: auto-selection returned empty task id" >&2
    exit 21
  }
  echo "selected task: $task_id (mode=$selected_mode, assignee=${selected_assignee:-unassigned}, age=${selected_age}m)" >&2
  echo "title: ${selected_title:-"(untitled)"}" >&2
fi

hb_args=(task-heartbeat --id "$task_id" --agent "$agent")
if [[ -n "$project_key" ]]; then
  hb_args+=(--project-key "$project_key")
fi
if [[ -n "$note" ]]; then
  hb_args+=(--note "$note")
fi

if [[ "$dry_run" -eq 1 ]]; then
  printf 'DRY RUN:'
  printf ' %q' "$COORD_BIN" "${hb_args[@]}"
  printf '\n'
  exit 0
fi

"$COORD_BIN" "${hb_args[@]}"
echo "heartbeat sent: task_id=$task_id agent=$agent"
