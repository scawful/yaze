#!/bin/bash

# Simple lock/lease helper for multi-agent Ralph loops.

set -euo pipefail

ROOT_DIR="$(pwd)"
LOCK_DIR=".claude/locks"
ACTION=""
AREA=""
OWNER="${USER:-unknown}"
TTL_MIN=180
FORCE=0

usage() {
  cat <<'EOF'
Ralph loop lock helper

USAGE:
  scripts/agents/ralph-loop-lock.sh <action> [options]

ACTIONS:
  acquire            Acquire a lock for an area
  release            Release a lock for an area
  status             Show lock status (all or a single area)

OPTIONS:
  --area <name>       Lock area name (required for acquire/release)
  --owner <name>      Owner identifier (default: $USER)
  --ttl <minutes>     Lease duration in minutes (default: 180)
  --force             Force takeover if lock is expired
  --root <dir>        Workspace root (default: pwd)
  -h, --help          Show help
EOF
}

if [[ $# -eq 0 ]]; then
  usage
  exit 1
fi

case "${1:-}" in
  acquire|release|status)
    ACTION="$1"
    shift
    ;;
  -h|--help)
    usage
    exit 0
    ;;
esac

while [[ $# -gt 0 ]]; do
  case "$1" in
    --area)
      AREA="$2"
      shift 2
      ;;
    --owner)
      OWNER="$2"
      shift 2
      ;;
    --ttl)
      if [[ -z "${2:-}" ]]; then
        echo "Error: --ttl requires a number" >&2
        exit 1
      fi
      if ! [[ "$2" =~ ^[0-9]+$ ]]; then
        echo "Error: --ttl must be an integer" >&2
        exit 1
      fi
      TTL_MIN="$2"
      shift 2
      ;;
    --force)
      FORCE=1
      shift
      ;;
    --root)
      ROOT_DIR="$2"
      shift 2
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

if [[ -z "$ACTION" ]]; then
  echo "Error: action required (acquire|release|status)" >&2
  usage
  exit 1
fi

ROOT_DIR=$(cd "$ROOT_DIR" && pwd)
LOCK_DIR="$ROOT_DIR/$LOCK_DIR"

lock_file() {
  echo "$LOCK_DIR/${AREA}.lock"
}

read_lock_field() {
  local file="$1"
  local key="$2"
  awk -F': ' -v k="$key" '$1==k {print $2}' "$file" 2>/dev/null || true
}

format_epoch() {
  local epoch="$1"
  if [[ "$(uname)" == "Darwin" ]]; then
    date -u -r "$epoch" +%Y-%m-%dT%H:%M:%SZ
  else
    date -u -d "@$epoch" +%Y-%m-%dT%H:%M:%SZ
  fi
}

if [[ "$ACTION" == "status" ]]; then
  if [[ -n "$AREA" ]]; then
    file="$(lock_file)"
    if [[ -f "$file" ]]; then
      cat "$file"
    else
      echo "No lock for area: $AREA"
    fi
  else
    if [[ ! -d "$LOCK_DIR" ]]; then
      echo "No locks."
      exit 0
    fi
    for file in "$LOCK_DIR"/*.lock; do
      [[ -f "$file" ]] || continue
      area="$(basename "$file" .lock)"
      owner="$(read_lock_field "$file" owner)"
      expires="$(read_lock_field "$file" expires_at)"
      echo "$area :: owner=${owner:-unknown} expires=${expires:-unknown}"
    done
  fi
  exit 0
fi

if [[ -z "$AREA" ]]; then
  echo "Error: --area is required for $ACTION" >&2
  exit 1
fi

file="$(lock_file)"
mkdir -p "$LOCK_DIR"

now_epoch="$(date -u +%s)"

if [[ "$ACTION" == "release" ]]; then
  if [[ -f "$file" ]]; then
    rm -f "$file"
    echo "Released lock: $AREA"
  else
    echo "No lock to release for area: $AREA"
  fi
  exit 0
fi

if [[ -f "$file" ]]; then
  expires_epoch="$(read_lock_field "$file" expires_epoch)"
  if [[ -n "$expires_epoch" ]] && [[ "$expires_epoch" =~ ^[0-9]+$ ]] && [[ "$expires_epoch" -gt "$now_epoch" ]]; then
    echo "Lock already held for area: $AREA"
    cat "$file"
    exit 1
  fi
  if [[ $FORCE -eq 0 ]]; then
    echo "Lock expired for area: $AREA (use --force to take over)"
    cat "$file"
    exit 1
  fi
fi

expires_epoch=$((now_epoch + TTL_MIN * 60))
created_at="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
expires_at="$(format_epoch "$expires_epoch")"

cat > "$file" <<EOF
owner: $OWNER
pid: $$
created_at: $created_at
created_epoch: $now_epoch
ttl_minutes: $TTL_MIN
expires_at: $expires_at
expires_epoch: $expires_epoch
EOF

echo "Locked $AREA until $expires_at"
