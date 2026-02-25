#!/usr/bin/env bash
# Thin compatibility wrapper around import-coordination-board.py.
# Keeps a stable shell entrypoint while using the Python implementation
# as the single source of truth.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PY_SCRIPT="$SCRIPT_DIR/import-coordination-board.py"

if [[ ! -f "$PY_SCRIPT" ]]; then
  echo "error: missing importer: $PY_SCRIPT" >&2
  exit 1
fi

# Safety default: dry-run unless caller explicitly asks for --apply.
if [[ $# -eq 0 ]]; then
  exec python3 "$PY_SCRIPT" --dry-run
fi

exec python3 "$PY_SCRIPT" "$@"
