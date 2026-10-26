#!/usr/bin/env bash
# Compatibility entrypoint: keep Gemini builds on the same quiet, locked path.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
exec "$ROOT/scripts/agent_build.sh" "$@"
