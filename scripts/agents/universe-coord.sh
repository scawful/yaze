#!/usr/bin/env bash
# universe-coord.sh
#
# Local-first, cross-project agent coordination.
# Source of truth:
#   - $UNIVERSE_DIR/events.jsonl   (append-only event log)
#   - $UNIVERSE_DIR/state.json     (materialized view)
#
# No external service required.

set -euo pipefail

UNIVERSE_DIR="${UNIVERSE_DIR:-$HOME/.context/agent-universe}"
EVENTS_FILE="$UNIVERSE_DIR/events.jsonl"
STATE_FILE="$UNIVERSE_DIR/state.json"
LOCK_DIR="$UNIVERSE_DIR/.lock"

usage() {
  cat <<'EOF'
Usage: universe-coord.sh <command> [options]

Commands:
  init
  rebuild-state
  task-add --title <text> [--agent <id>] [--project-key <key>] [--priority <A|B|C>] [--note <text>] [--tags <a,b,c>]
  task-list [--project-key <key>] [--status <open|active|complete|all>] [--format <text|json>]
  task-claim --id <task_id> --agent <id> [--note <text>]
  task-heartbeat --id <task_id> --agent <id> [--note <text>]
  task-handoff --id <task_id> --agent <id> --to <id> [--note <text>]
  task-complete --id <task_id> --agent <id> [--note <text>]
  task-generate-board [--project-key <key>] [--out <path>]

Environment:
  UNIVERSE_DIR   root directory for events/state (default: ~/.context/agent-universe)
  PROJECT_ROOT   optional project path used by default project-key derivation
EOF
}

now_iso_utc() {
  date -u +"%Y-%m-%dT%H:%M:%SZ"
}

default_project_key() {
  local root="${PROJECT_ROOT:-$(pwd)}"
  root="$(cd "$root" && pwd)"
  printf "%s" "$root" | sed 's#^/##; s#/#:#g'
}

new_id() {
  local prefix="$1"
  printf "%s_%s_%s" "$prefix" "$(date -u +%Y%m%dT%H%M%SZ)" "$RANDOM"
}

ensure_storage() {
  mkdir -p "$UNIVERSE_DIR"
  touch "$EVENTS_FILE"
  if [[ ! -f "$STATE_FILE" ]]; then
    cat >"$STATE_FILE" <<'EOF'
{"version":1,"updated_at":"","tasks":{}}
EOF
  fi
}

acquire_lock() {
  local attempts=0
  while ! mkdir "$LOCK_DIR" 2>/dev/null; do
    attempts=$((attempts + 1))
    if (( attempts > 400 )); then
      echo "error: timeout acquiring lock: $LOCK_DIR" >&2
      return 1
    fi
    sleep 0.05
  done
}

release_lock() {
  rmdir "$LOCK_DIR" 2>/dev/null || true
}

rebuild_state_locked() {
  python3 - "$EVENTS_FILE" "$STATE_FILE" <<'PY'
import json
import os
import sys
from datetime import datetime, timezone

events_path = sys.argv[1]
state_path = sys.argv[2]

tasks = {}

def now_iso():
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

def task_default(task_id):
    return {
        "task_id": task_id,
        "title": "",
        "project_key": "",
        "priority": "B",
        "tags": [],
        "status": "open",
        "assignee": "",
        "created_at": "",
        "updated_at": "",
        "completed_at": "",
        "last_note": "",
        "history": [],
    }

with open(events_path, "r", encoding="utf-8") as f:
    for raw in f:
        raw = raw.strip()
        if not raw:
            continue
        try:
            event = json.loads(raw)
        except json.JSONDecodeError:
            continue

        task_id = event.get("task_id")
        if not task_id:
            continue
        task = tasks.setdefault(task_id, task_default(task_id))
        etype = event.get("type", "")
        ts = event.get("ts", "")

        task["updated_at"] = ts or task["updated_at"]
        task["project_key"] = event.get("project_key", task["project_key"])
        task["last_note"] = event.get("note", task["last_note"])
        task["history"].append({
            "event_id": event.get("event_id", ""),
            "type": etype,
            "ts": ts,
            "agent": event.get("agent", ""),
            "note": event.get("note", ""),
        })

        if etype == "task_added":
            task["title"] = event.get("title", task["title"])
            task["priority"] = event.get("priority", task["priority"])
            task["tags"] = event.get("tags", task["tags"])
            task["status"] = "open"
            task["assignee"] = event.get("agent", task["assignee"])
            task["created_at"] = ts or task["created_at"]
        elif etype == "task_claimed":
            task["status"] = "active"
            task["assignee"] = event.get("agent", task["assignee"])
        elif etype == "task_heartbeat":
            if task["status"] == "open":
                task["status"] = "active"
            if event.get("agent"):
                task["assignee"] = event.get("agent")
        elif etype == "task_handoff":
            task["status"] = "open"
            task["assignee"] = event.get("to", "")
        elif etype == "task_completed":
            task["status"] = "complete"
            task["completed_at"] = ts
            if event.get("agent"):
                task["assignee"] = event.get("agent")

state = {
    "version": 1,
    "updated_at": now_iso(),
    "tasks": tasks,
}

tmp_path = state_path + ".tmp"
with open(tmp_path, "w", encoding="utf-8") as out:
    json.dump(state, out, indent=2, sort_keys=True)
    out.write("\n")
os.replace(tmp_path, state_path)
PY
}

append_event_line() {
  local line="$1"
  if [[ -z "$line" ]]; then
    echo "error: refusing to append empty event line" >&2
    return 2
  fi
  if ! python3 - "$line" <<'PY'
import json
import sys

json.loads(sys.argv[1])
PY
  then
    echo "error: refusing to append invalid JSON event" >&2
    return 2
  fi
  ensure_storage
  acquire_lock
  trap release_lock RETURN
  printf "%s\n" "$line" >> "$EVENTS_FILE"
  rebuild_state_locked
}

read_state_json() {
  ensure_storage
  cat "$STATE_FILE"
}

emit_event_json() {
  # Reads required fields from env; prints single-line JSON.
  python3 - <<'PY'
import json
import os

def split_tags(raw):
    if not raw:
        return []
    return [t.strip() for t in raw.split(",") if t.strip()]

evt = {
    "event_id": os.environ["EV_EVENT_ID"],
    "type": os.environ["EV_TYPE"],
    "ts": os.environ["EV_TS"],
    "task_id": os.environ["EV_TASK_ID"],
    "project_key": os.environ["EV_PROJECT_KEY"],
    "agent": os.environ.get("EV_AGENT", ""),
    "to": os.environ.get("EV_TO", ""),
    "title": os.environ.get("EV_TITLE", ""),
    "priority": os.environ.get("EV_PRIORITY", "B"),
    "tags": split_tags(os.environ.get("EV_TAGS", "")),
    "note": os.environ.get("EV_NOTE", ""),
    "source": os.environ.get("EV_SOURCE", "universe-coord.sh"),
}
print(json.dumps(evt, separators=(",", ":"), ensure_ascii=False))
PY
}

task_add() {
  local title="" agent="" project_key="" priority="B" note="" tags=""
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --title) title="${2:-}"; shift 2 ;;
      --agent) agent="${2:-}"; shift 2 ;;
      --project-key) project_key="${2:-}"; shift 2 ;;
      --priority) priority="${2:-}"; shift 2 ;;
      --note) note="${2:-}"; shift 2 ;;
      --tags) tags="${2:-}"; shift 2 ;;
      *) echo "error: unknown arg: $1" >&2; return 2 ;;
    esac
  done
  [[ -n "$title" ]] || { echo "error: --title is required" >&2; return 2; }
  [[ -n "$project_key" ]] || project_key="$(default_project_key)"
  [[ -n "$agent" ]] || agent="unassigned"

  local task_id
  task_id="$(new_id task)"
  local event_id
  event_id="$(new_id evt)"
  local event_json

  event_json="$(
    EV_EVENT_ID="$event_id" \
    EV_TYPE="task_added" \
    EV_TS="$(now_iso_utc)" \
    EV_TASK_ID="$task_id" \
    EV_PROJECT_KEY="$project_key" \
    EV_AGENT="$agent" \
    EV_TITLE="$title" \
    EV_PRIORITY="$priority" \
    EV_TAGS="$tags" \
    EV_NOTE="$note" \
    emit_event_json
  )"
  append_event_line "$event_json"

  echo "$task_id"
}

task_claim() {
  local id="" agent="" project_key="" note=""
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --id) id="${2:-}"; shift 2 ;;
      --agent) agent="${2:-}"; shift 2 ;;
      --project-key) project_key="${2:-}"; shift 2 ;;
      --note) note="${2:-}"; shift 2 ;;
      *) echo "error: unknown arg: $1" >&2; return 2 ;;
    esac
  done
  [[ -n "$id" && -n "$agent" ]] || { echo "error: --id and --agent are required" >&2; return 2; }
  [[ -n "$project_key" ]] || project_key="$(default_project_key)"
  local event_json

  event_json="$(
    EV_EVENT_ID="$(new_id evt)" \
    EV_TYPE="task_claimed" \
    EV_TS="$(now_iso_utc)" \
    EV_TASK_ID="$id" \
    EV_PROJECT_KEY="$project_key" \
    EV_AGENT="$agent" \
    EV_NOTE="$note" \
    emit_event_json
  )"
  append_event_line "$event_json"
}

task_heartbeat() {
  local id="" agent="" project_key="" note=""
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --id) id="${2:-}"; shift 2 ;;
      --agent) agent="${2:-}"; shift 2 ;;
      --project-key) project_key="${2:-}"; shift 2 ;;
      --note) note="${2:-}"; shift 2 ;;
      *) echo "error: unknown arg: $1" >&2; return 2 ;;
    esac
  done
  [[ -n "$id" && -n "$agent" ]] || { echo "error: --id and --agent are required" >&2; return 2; }
  [[ -n "$project_key" ]] || project_key="$(default_project_key)"
  local event_json

  event_json="$(
    EV_EVENT_ID="$(new_id evt)" \
    EV_TYPE="task_heartbeat" \
    EV_TS="$(now_iso_utc)" \
    EV_TASK_ID="$id" \
    EV_PROJECT_KEY="$project_key" \
    EV_AGENT="$agent" \
    EV_NOTE="$note" \
    emit_event_json
  )"
  append_event_line "$event_json"
}

task_handoff() {
  local id="" agent="" to="" project_key="" note=""
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --id) id="${2:-}"; shift 2 ;;
      --agent) agent="${2:-}"; shift 2 ;;
      --to) to="${2:-}"; shift 2 ;;
      --project-key) project_key="${2:-}"; shift 2 ;;
      --note) note="${2:-}"; shift 2 ;;
      *) echo "error: unknown arg: $1" >&2; return 2 ;;
    esac
  done
  [[ -n "$id" && -n "$agent" && -n "$to" ]] || {
    echo "error: --id, --agent, and --to are required" >&2
    return 2
  }
  [[ -n "$project_key" ]] || project_key="$(default_project_key)"
  local event_json

  event_json="$(
    EV_EVENT_ID="$(new_id evt)" \
    EV_TYPE="task_handoff" \
    EV_TS="$(now_iso_utc)" \
    EV_TASK_ID="$id" \
    EV_PROJECT_KEY="$project_key" \
    EV_AGENT="$agent" \
    EV_TO="$to" \
    EV_NOTE="$note" \
    emit_event_json
  )"
  append_event_line "$event_json"
}

task_complete() {
  local id="" agent="" project_key="" note=""
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --id) id="${2:-}"; shift 2 ;;
      --agent) agent="${2:-}"; shift 2 ;;
      --project-key) project_key="${2:-}"; shift 2 ;;
      --note) note="${2:-}"; shift 2 ;;
      *) echo "error: unknown arg: $1" >&2; return 2 ;;
    esac
  done
  [[ -n "$id" && -n "$agent" ]] || { echo "error: --id and --agent are required" >&2; return 2; }
  [[ -n "$project_key" ]] || project_key="$(default_project_key)"
  local event_json

  event_json="$(
    EV_EVENT_ID="$(new_id evt)" \
    EV_TYPE="task_completed" \
    EV_TS="$(now_iso_utc)" \
    EV_TASK_ID="$id" \
    EV_PROJECT_KEY="$project_key" \
    EV_AGENT="$agent" \
    EV_NOTE="$note" \
    emit_event_json
  )"
  append_event_line "$event_json"
}

task_list() {
  local project_key="" status="all" format="text"
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --project-key) project_key="${2:-}"; shift 2 ;;
      --status) status="${2:-}"; shift 2 ;;
      --format) format="${2:-}"; shift 2 ;;
      *) echo "error: unknown arg: $1" >&2; return 2 ;;
    esac
  done

  ensure_storage
  python3 - "$STATE_FILE" "$project_key" "$status" "$format" <<'PY'
import json
import sys

state_path = sys.argv[1]
project_key = sys.argv[2]
status_filter = sys.argv[3]
fmt = sys.argv[4]
with open(state_path, "r", encoding="utf-8") as f:
    state = json.load(f)
tasks = list(state.get("tasks", {}).values())

if project_key:
    tasks = [t for t in tasks if t.get("project_key") == project_key]
if status_filter != "all":
    tasks = [t for t in tasks if t.get("status") == status_filter]

tasks.sort(key=lambda t: (t.get("updated_at", ""), t.get("task_id", "")), reverse=True)

if fmt == "json":
    print(json.dumps(tasks, indent=2))
    raise SystemExit(0)

if not tasks:
    print("(no tasks)")
    raise SystemExit(0)

print("task_id\tstatus\tassignee\tpriority\ttitle")
for t in tasks:
    print(
        f"{t.get('task_id','')}\t"
        f"{t.get('status','')}\t"
        f"{t.get('assignee','')}\t"
        f"{t.get('priority','')}\t"
        f"{t.get('title','')}"
    )
PY
}

task_generate_board() {
  local project_key="" out_path=""
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --project-key) project_key="${2:-}"; shift 2 ;;
      --out) out_path="${2:-}"; shift 2 ;;
      *) echo "error: unknown arg: $1" >&2; return 2 ;;
    esac
  done

  ensure_storage
  local markdown
  markdown="$(python3 - "$STATE_FILE" "$project_key" <<'PY'
import json
import sys
from datetime import datetime, timezone

state_path = sys.argv[1]
project_key = sys.argv[2]
with open(state_path, "r", encoding="utf-8") as f:
    state = json.load(f)
tasks = list(state.get("tasks", {}).values())
if project_key:
    tasks = [t for t in tasks if t.get("project_key") == project_key]

def sort_desc(items):
    return sorted(items, key=lambda t: (t.get("updated_at",""), t.get("task_id","")), reverse=True)

active = sort_desc([t for t in tasks if t.get("status") == "active"])
open_tasks = sort_desc([t for t in tasks if t.get("status") == "open"])
complete = sort_desc([t for t in tasks if t.get("status") == "complete"])[:20]

generated = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
lines = []
lines.append("# Coordination Snapshot (Generated)")
lines.append("")
lines.append(f"- Generated: `{generated}`")
lines.append(f"- Source: `~/.context/agent-universe/state.json`")
if project_key:
    lines.append(f"- Project Key: `{project_key}`")
lines.append("")

def add_section(title, entries):
    lines.append(f"## {title}")
    if not entries:
        lines.append("- (none)")
        lines.append("")
        return
    for t in entries:
        lines.append(
            f"- `{t.get('task_id','')}` [{t.get('priority','B')}] "
            f"{t.get('title','(untitled)')} "
            f"(assignee: `{t.get('assignee','') or 'unassigned'}`, "
            f"updated: `{t.get('updated_at','')}`)"
        )
    lines.append("")

add_section("Active", active)
add_section("Open", open_tasks)
add_section("Recently Completed", complete)
print("\n".join(lines))
PY
)"

  if [[ -n "$out_path" ]]; then
    mkdir -p "$(dirname "$out_path")"
    printf "%s\n" "$markdown" > "$out_path"
    echo "$out_path"
  else
    printf "%s\n" "$markdown"
  fi
}

cmd_init() {
  ensure_storage
  acquire_lock
  trap release_lock RETURN
  rebuild_state_locked
  echo "initialized: $UNIVERSE_DIR"
}

cmd_rebuild_state() {
  ensure_storage
  acquire_lock
  trap release_lock RETURN
  rebuild_state_locked
  echo "rebuilt: $STATE_FILE"
}

main() {
  local cmd="${1:-}"
  if [[ -z "$cmd" ]]; then
    usage
    exit 2
  fi
  shift || true

  case "$cmd" in
    init) cmd_init "$@" ;;
    rebuild-state) cmd_rebuild_state "$@" ;;
    task-add) task_add "$@" ;;
    task-list) task_list "$@" ;;
    task-claim) task_claim "$@" ;;
    task-heartbeat) task_heartbeat "$@" ;;
    task-handoff) task_handoff "$@" ;;
    task-complete) task_complete "$@" ;;
    task-generate-board) task_generate_board "$@" ;;
    -h|--help|help) usage ;;
    *)
      echo "error: unknown command: $cmd" >&2
      usage
      exit 2
      ;;
  esac
}

main "$@"
