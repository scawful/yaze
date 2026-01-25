#!/usr/bin/env bash

# Stop Ralph loop processes (Codex loop) and optionally kill the tmux session.

set -euo pipefail

ROOT_DIR="$(pwd)"
STATE_FILE=""
TMUX_SESSION="ralph-yaze"
FORCE=0

usage() {
  cat <<'EOF'
Stop Ralph loop processes (Codex loop)

USAGE:
  scripts/agents/ralph-loop-stop.sh [options]

OPTIONS:
  --root <dir>         Workspace root (default: pwd)
  --state-file <path>  State file path (default: .claude/ralph-loop.codex.md)
  --tmux-session <s>   tmux session to kill (default: ralph-yaze)
  --force              Send SIGKILL if processes do not exit
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
    --tmux-session)
      TMUX_SESSION="$2"
      shift 2
      ;;
    --force)
      FORCE=1
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
if [[ -z "$STATE_FILE" ]]; then
  STATE_FILE="$ROOT_DIR/.claude/ralph-loop.codex.md"
fi

read_state_field() {
  local key="$1"
  awk -F': ' -v k="$key" '
    $0=="---" {
      if(frontmatter==0) {frontmatter=1; next}
      exit
    }
    frontmatter==1 && $1==k {gsub(/"/,"",$2); print $2; exit}
  ' "$STATE_FILE" 2>/dev/null || true
}

kill_group() {
  local pgid="$1"
  if [[ -z "$pgid" || ! "$pgid" =~ ^[0-9]+$ ]]; then
    return 0
  fi
  if kill -0 "-$pgid" 2>/dev/null; then
    kill -TERM "-$pgid" 2>/dev/null || true
    for _ in {1..20}; do
      if ! kill -0 "-$pgid" 2>/dev/null; then
        return 0
      fi
      sleep 0.2
    done
    if [[ $FORCE -eq 1 ]]; then
      kill -KILL "-$pgid" 2>/dev/null || true
    fi
  fi
}

kill_pid() {
  local pid="$1"
  if [[ -z "$pid" || ! "$pid" =~ ^[0-9]+$ ]]; then
    return 0
  fi
  if kill -0 "$pid" 2>/dev/null; then
    kill -TERM "$pid" 2>/dev/null || true
    for _ in {1..20}; do
      if ! kill -0 "$pid" 2>/dev/null; then
        return 0
      fi
      sleep 0.2
    done
    if [[ $FORCE -eq 1 ]]; then
      kill -KILL "$pid" 2>/dev/null || true
    fi
  fi
}

if [[ -f "$STATE_FILE" ]]; then
  runner_pgid="$(read_state_field runner_pgid)"
  runner_pid="$(read_state_field runner_pid)"
  kill_group "$runner_pgid"
  kill_pid "$runner_pid"
fi

pgrep -f "ralph-loop-codex.sh" >/dev/null 2>&1 && pkill -TERM -f "ralph-loop-codex.sh" 2>/dev/null || true
if [[ $FORCE -eq 1 ]]; then
  pgrep -f "ralph-loop-codex.sh" >/dev/null 2>&1 && pkill -KILL -f "ralph-loop-codex.sh" 2>/dev/null || true
fi

if command -v tmux >/dev/null 2>&1; then
  if [[ -n "$TMUX_SESSION" ]] && tmux has-session -t "$TMUX_SESSION" 2>/dev/null; then
    tmux kill-session -t "$TMUX_SESSION"
  fi
fi

echo "Ralph loop stop requested."
