#!/usr/bin/env bash

# Wrapper for the consolidated Ralph loop status helper.

set -euo pipefail

ROOT_DIR="$(pwd)"
STATE_FILE=""
SHOW_ALL=0
OOS_ROOT="${RALPH_OOS_ROOT:-$HOME/src/hobby/oracle-of-secrets}"

usage() {
  cat <<'EOF'
Ralph loop status helper (consolidated)

USAGE:
  scripts/agents/ralph-loop-status.sh [options]

OPTIONS:
  --root <dir>         Workspace root (default: pwd)
  --state-file <path>  State file path (default: .claude/ralph-loop.codex.md)
  --all                Print full YAML front matter
  -h, --help           Show help
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --root)
      ROOT_DIR="$2"
      shift 2
      ;;
    --state-file)
      STATE_FILE="$2"
      shift 2
      ;;
    --all)
      SHOW_ALL=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Error: unknown option $1" >&2
      usage
      exit 1
      ;;
  esac
done

ROOT_DIR=$(cd "$ROOT_DIR" && pwd)
if [[ -d "$OOS_ROOT" ]]; then
  OOS_ROOT=$(cd "$OOS_ROOT" && pwd)
fi
if [[ -z "$STATE_FILE" ]]; then
  STATE_FILE="$ROOT_DIR/.claude/ralph-loop.codex.md"
fi

STATUS_SCRIPT="$OOS_ROOT/scripts/ralph-loop-status.sh"
if [[ ! -x "$STATUS_SCRIPT" ]]; then
  echo "Error: consolidated status script not found: $STATUS_SCRIPT" >&2
  echo "Tip: set RALPH_OOS_ROOT to the oracle-of-secrets repo root." >&2
  exit 1
fi

ARGS=(--root "$ROOT_DIR" --state-file "$STATE_FILE")
if [[ $SHOW_ALL -eq 1 ]]; then
  ARGS+=(--all)
fi

exec "$STATUS_SCRIPT" "${ARGS[@]}"
