#!/usr/bin/env bash
# Wrapper for triggering GitHub Actions workflows via gh CLI.
# Usage: scripts/agents/run-gh-workflow.sh <workflow_file> [--ref <ref>] [key=value ...]

set -euo pipefail

if ! command -v gh >/dev/null 2>&1; then
  echo "error: gh CLI is required (https://cli.github.com/)" >&2
  exit 1
fi

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <workflow_file> [--ref <ref>] [key=value ...]" >&2
  exit 1
fi

WORKFLOW="$1"
shift

REF=""
INPUT_ARGS=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    --ref)
      REF="$2"
      shift 2
      ;;
    *)
      INPUT_ARGS+=("-f" "$1")
      shift
      ;;
  esac
done

CMD=(gh workflow run "$WORKFLOW")
if [[ -n "$REF" ]]; then
  CMD+=("--ref" "$REF")
fi
if [[ ${#INPUT_ARGS[@]} -gt 0 ]]; then
  CMD+=("${INPUT_ARGS[@]}")
fi

echo "+ ${CMD[*]}"
"${CMD[@]}"

RUN_URL=$(gh run list --workflow "$WORKFLOW" --limit 1 --json url -q '.[0].url')
if [[ -n "$RUN_URL" ]]; then
  echo "Triggered workflow. Track progress at: $RUN_URL"
fi
