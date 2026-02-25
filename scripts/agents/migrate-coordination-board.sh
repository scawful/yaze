#!/usr/bin/env bash
# migrate-coordination-board.sh
#
# Safe migration helper:
# 1) Always runs import dry-run preview first.
# 2) Applies import only if --apply is passed.
# 3) Generates a coordination snapshot markdown after apply.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
IMPORT_SH="$SCRIPT_DIR/import-coordination-board.sh"
COORD_SH="$SCRIPT_DIR/universe-coord.sh"

BOARD_PATH="docs/internal/agents/coordination-board.md"
OUT_PATH="docs/internal/agents/coordination-board.generated.md"
PROJECT_KEY=""
LIMIT=""
APPLY=false
UNIVERSE_DIR_OVERRIDE="${UNIVERSE_DIR:-}"
DRY_JSON=""
APPLY_JSON=""

usage() {
  cat <<'EOF'
Usage: migrate-coordination-board.sh [options]

Options:
  --board <path>        Legacy board markdown path
  --out <path>          Generated board snapshot path
  --project-key <key>   Project namespace key
  --limit <n>           Import at most N board entries
  --universe-dir <dir>  Universe coordination root (default: env/default)
  --dry-run             Explicit dry-run mode (default behavior)
  --apply               Apply import + generate snapshot
  --help                Show this help

Behavior:
  - Dry-run preview always runs first.
  - No state mutation happens unless --apply is provided.
EOF
}

require_scripts() {
  [[ -x "$IMPORT_SH" ]] || { echo "error: missing executable: $IMPORT_SH" >&2; exit 1; }
  [[ -x "$COORD_SH" ]] || { echo "error: missing executable: $COORD_SH" >&2; exit 1; }
}

cleanup_tmp() {
  [[ -n "${DRY_JSON:-}" ]] && rm -f "$DRY_JSON"
  [[ -n "${APPLY_JSON:-}" ]] && rm -f "$APPLY_JSON"
}

main() {
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --board) BOARD_PATH="${2:-}"; shift 2 ;;
      --out) OUT_PATH="${2:-}"; shift 2 ;;
      --project-key) PROJECT_KEY="${2:-}"; shift 2 ;;
      --limit) LIMIT="${2:-}"; shift 2 ;;
      --universe-dir) UNIVERSE_DIR_OVERRIDE="${2:-}"; shift 2 ;;
      --dry-run) APPLY=false; shift ;;
      --apply) APPLY=true; shift ;;
      -h|--help|help) usage; exit 0 ;;
      *) echo "error: unknown arg: $1" >&2; usage; exit 2 ;;
    esac
  done

  require_scripts
  [[ -f "$BOARD_PATH" ]] || { echo "error: board not found: $BOARD_PATH" >&2; exit 1; }

  DRY_JSON="$(mktemp)"
  APPLY_JSON="$(mktemp)"
  trap cleanup_tmp EXIT

  local import_args=()
  import_args+=(--board "$BOARD_PATH")
  [[ -n "$PROJECT_KEY" ]] && import_args+=(--project-key "$PROJECT_KEY")
  [[ -n "$LIMIT" ]] && import_args+=(--limit "$LIMIT")

  if [[ -n "$UNIVERSE_DIR_OVERRIDE" ]]; then
    import_args+=(--universe-dir "$UNIVERSE_DIR_OVERRIDE")
  fi

  # Always preview first.
  "$IMPORT_SH" "${import_args[@]}" --dry-run > "$DRY_JSON"

  python3 - <<'PY' "$DRY_JSON"
import json
import sys

with open(sys.argv[1], "r", encoding="utf-8") as f:
    data = json.load(f)
print("preview:")
print(f"  mode={data.get('mode')}")
print(f"  entries_found={data.get('entries_found')}")
print(f"  events_generated={data.get('events_generated')}")
print(f"  project_key={data.get('project_key')}")
PY

  if [[ "$APPLY" != true ]]; then
    echo "dry-run only. Re-run with --apply to append events and generate snapshot."
    exit 0
  fi

  "$IMPORT_SH" "${import_args[@]}" --apply > "$APPLY_JSON"

  python3 - <<'PY' "$APPLY_JSON"
import json
import sys

with open(sys.argv[1], "r", encoding="utf-8") as f:
    data = json.load(f)
print("apply:")
print(f"  events_appended={data.get('events_appended')}")
print(f"  events_skipped_existing={data.get('events_skipped_existing')}")
print(f"  events_file={data.get('events_file')}")
print(f"  state_file={data.get('state_file')}")
PY

  local gen_args=()
  [[ -n "$PROJECT_KEY" ]] && gen_args+=(--project-key "$PROJECT_KEY")
  gen_args+=(--out "$OUT_PATH")

  if [[ -n "$UNIVERSE_DIR_OVERRIDE" ]]; then
    UNIVERSE_DIR="$UNIVERSE_DIR_OVERRIDE" "$COORD_SH" task-generate-board "${gen_args[@]}" >/dev/null
  else
    "$COORD_SH" task-generate-board "${gen_args[@]}" >/dev/null
  fi

  echo "snapshot: $OUT_PATH"
}

main "$@"
