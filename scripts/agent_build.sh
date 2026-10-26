#!/bin/bash
# Quiet incremental builds only. This helper never runs tests or installs.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec python3 "$SCRIPT_DIR/agents/agent_build.py" "$@"
