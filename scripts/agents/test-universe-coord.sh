#!/usr/bin/env bash
# test-universe-coord.sh
#
# Script-level validation for universe coordination tooling.
# Runs in temp directories and does not touch real ~/.context state.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

UNIVERSE_COORD="$SCRIPT_DIR/universe-coord.sh"
COORD_WRAPPER="$SCRIPT_DIR/coord"
COORD_HEARTBEAT="$SCRIPT_DIR/coord-heartbeat.sh"
IMPORT_PY="$SCRIPT_DIR/import-coordination-board.py"
IMPORT_SH="$SCRIPT_DIR/import-coordination-board.sh"
MIGRATE_SH="$SCRIPT_DIR/migrate-coordination-board.sh"

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

require_exec() {
  local p="$1"
  [[ -x "$p" ]] || fail "missing executable: $p"
}

require_file() {
  local p="$1"
  [[ -f "$p" ]] || fail "missing file: $p"
}

check_syntax() {
  bash -n "$UNIVERSE_COORD"
  bash -n "$COORD_WRAPPER"
  bash -n "$COORD_HEARTBEAT"
  bash -n "$IMPORT_SH"
  bash -n "$MIGRATE_SH"
  python3 -m py_compile "$IMPORT_PY"
}

main() {
  require_exec "$UNIVERSE_COORD"
  require_exec "$COORD_WRAPPER"
  require_exec "$COORD_HEARTBEAT"
  require_exec "$IMPORT_SH"
  require_exec "$MIGRATE_SH"
  require_file "$IMPORT_PY"

  check_syntax

  local tmp_universe tmp_out
  tmp_universe="$(mktemp -d)"
  tmp_out="$(mktemp -d)"

  export UNIVERSE_DIR="$tmp_universe"

  "$UNIVERSE_COORD" init >/dev/null

  local task_id
  task_id="$("$UNIVERSE_COORD" task-add \
    --title "script-test task" \
    --agent "ai-infra-architect" \
    --project-key "test:project" \
    --priority A \
    --note "seed")"

  "$UNIVERSE_COORD" task-claim --id "$task_id" --agent "ai-infra-architect" --project-key "test:project" --note "claim" >/dev/null
  "$UNIVERSE_COORD" task-heartbeat --id "$task_id" --agent "ai-infra-architect" --project-key "test:project" --note "hb" >/dev/null
  "$UNIVERSE_COORD" task-handoff --id "$task_id" --agent "ai-infra-architect" --to "imgui-frontend-engineer" --project-key "test:project" --note "handoff" >/dev/null
  "$UNIVERSE_COORD" task-complete --id "$task_id" --agent "imgui-frontend-engineer" --project-key "test:project" --note "done" >/dev/null

  "$UNIVERSE_COORD" task-list --project-key "test:project" --status all --format json > "$tmp_out/list.json"
  "$UNIVERSE_COORD" task-generate-board --project-key "test:project" --out "$tmp_out/board.md" >/dev/null

  python3 "$IMPORT_PY" \
    --board "$ROOT_DIR/docs/internal/agents/coordination-board.md" \
    --universe-dir "$tmp_universe" \
    --project-key "test:project" \
    --limit 2 \
    --dry-run > "$tmp_out/import-dry.json"

  python3 "$IMPORT_PY" \
    --board "$ROOT_DIR/docs/internal/agents/coordination-board.md" \
    --universe-dir "$tmp_universe" \
    --project-key "test:project" \
    --limit 2 \
    --apply > "$tmp_out/import-apply.json"

  "$IMPORT_SH" \
    --board "$ROOT_DIR/docs/internal/agents/coordination-board.md" \
    --universe-dir "$tmp_universe" \
    --project-key "test:project" \
    --limit 1 \
    --dry-run > "$tmp_out/import-wrapper-dry.json"

  "$COORD_WRAPPER" task-add --title "wrapper task" --agent "ai-infra-architect" >/dev/null
  "$COORD_WRAPPER" task-list --status all --format json > "$tmp_out/wrapper-list.json"

  local helper_recent helper_other
  helper_recent="$("$UNIVERSE_COORD" task-add \
    --title "helper-recent-task" \
    --agent "ai-infra-architect" \
    --project-key "test:project" \
    --priority B)"
  "$UNIVERSE_COORD" task-claim --id "$helper_recent" --agent "ai-infra-architect" --project-key "test:project" >/dev/null

  helper_other="$("$UNIVERSE_COORD" task-add \
    --title "helper-other-task" \
    --agent "imgui-frontend-engineer" \
    --project-key "test:project" \
    --priority B)"
  "$UNIVERSE_COORD" task-claim --id "$helper_other" --agent "imgui-frontend-engineer" --project-key "test:project" >/dev/null

  "$COORD_HEARTBEAT" --agent "ai-infra-architect" --project-key "test:project" --note "auto-hb" > "$tmp_out/helper-heartbeat.txt"
  "$COORD_HEARTBEAT" --agent "ai-infra-architect" --id "$helper_recent" --project-key "test:project" --dry-run > "$tmp_out/helper-heartbeat-dry.txt"

  "$MIGRATE_SH" \
    --board "$ROOT_DIR/docs/internal/agents/coordination-board.md" \
    --project-key "test:project" \
    --universe-dir "$tmp_universe" \
    --out "$tmp_out/generated.md" \
    --limit 1 > "$tmp_out/migrate-dry.txt"

  "$MIGRATE_SH" \
    --board "$ROOT_DIR/docs/internal/agents/coordination-board.md" \
    --project-key "test:project" \
    --universe-dir "$tmp_universe" \
    --out "$tmp_out/generated.md" \
    --limit 1 \
    --apply > "$tmp_out/migrate-apply.txt"

  python3 - "$tmp_universe" "$tmp_out" "$helper_recent" <<'PY'
import json
import pathlib
import sys

u = pathlib.Path(sys.argv[1])
o = pathlib.Path(sys.argv[2])

state = json.loads((u / "state.json").read_text())
assert state.get("tasks"), "state has no tasks"

tasks = json.loads((o / "list.json").read_text())
assert tasks and tasks[0]["status"] == "complete", "task lifecycle status mismatch"

board = (o / "board.md").read_text()
assert "Coordination Snapshot (Generated)" in board

dry = json.loads((o / "import-dry.json").read_text())
assert dry["mode"] == "dry-run"
assert dry["entries_found"] >= 1

apply = json.loads((o / "import-apply.json").read_text())
assert apply["mode"] == "apply"
assert apply["events_generated"] >= apply["events_appended"]

wrapper = json.loads((o / "import-wrapper-dry.json").read_text())
assert wrapper["mode"] == "dry-run"

wrapper_tasks = json.loads((o / "wrapper-list.json").read_text())
assert any(t.get("project_key", "").startswith("Users:scawful:src:hobby:yaze") for t in wrapper_tasks), "wrapper default project key missing"

generated = (o / "generated.md").read_text()
assert "Coordination Snapshot (Generated)" in generated
assert "## Active" in generated and "## Open" in generated and "## Recently Completed" in generated

helper_out = (o / "helper-heartbeat.txt").read_text()
assert "heartbeat sent:" in helper_out, "helper heartbeat did not run"

helper_dry = (o / "helper-heartbeat-dry.txt").read_text()
assert "DRY RUN:" in helper_dry and "task-heartbeat" in helper_dry, "helper dry-run missing command"

state_after = json.loads((u / "state.json").read_text())
recent = state_after["tasks"].get(sys.argv[3], {})
assert recent, "helper recent task missing from state"
hb_events = [h for h in recent.get("history", []) if h.get("type") == "task_heartbeat" and h.get("note") == "auto-hb"]
assert hb_events, "helper heartbeat event not recorded on preferred task"

print("OK", len(state["tasks"]), len(tasks), apply["events_appended"])
PY

  echo "PASS: universe coordination scripts"
  echo "tmp_universe=$tmp_universe"
  echo "tmp_output=$tmp_out"
}

main "$@"
