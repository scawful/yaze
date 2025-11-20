#!/usr/bin/env bash
# Basic health check for the HTTP API server.
# Usage: scripts/agents/test-http-api.sh [host] [port]

set -euo pipefail

HOST="${1:-127.0.0.1}"
PORT="${2:-8080}"
URL="http://${HOST}:${PORT}/api/v1/health"

if ! command -v curl >/dev/null 2>&1; then
  echo "error: curl is required to test the HTTP API" >&2
  exit 1
fi

echo "Checking HTTP API health endpoint at ${URL}"

for attempt in {1..10}; do
  if curl -fsS "${URL}" >/dev/null; then
    echo "HTTP API responded successfully (attempt ${attempt})"
    exit 0
  fi
  echo "Attempt ${attempt} failed; retrying..."
  sleep 1
done

echo "error: HTTP API did not respond at ${URL}" >&2
exit 1
