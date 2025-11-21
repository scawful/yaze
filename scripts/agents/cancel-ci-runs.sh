#!/usr/bin/env bash
set -euo pipefail

branch="${1:-feat/http-api-phase2}"
limit="${2:-20}"
dry_run="${DRY_RUN:-false}"

if ! command -v gh >/dev/null 2>&1; then
  echo "[cancel-ci-runs] gh CLI is required" >&2
  exit 1
fi

echo "[cancel-ci-runs] Checking latest ${limit} runs for branch '${branch}'"
runs=$(gh run list --branch "$branch" --limit "$limit" --json databaseId,status,conclusion,workflowName,displayTitle --jq '.[] | select(.status != "completed") | [.databaseId, .status, .workflowName, .displayTitle] | @tsv')

if [[ -z "$runs" ]]; then
  echo "[cancel-ci-runs] No in-progress or queued runs found."
  exit 0
fi

while IFS=$'\t' read -r run_id status workflow title; do
  echo "[cancel-ci-runs] Found run ${run_id} (${workflow} – ${title}) status=${status}"
  if [[ "$dry_run" == "true" ]]; then
    echo "[cancel-ci-runs] DRY RUN – would cancel run ${run_id}"
  else
    gh run cancel "$run_id"
  fi
done <<< "$runs"

if [[ "$dry_run" == "true" ]]; then
  echo "[cancel-ci-runs] Completed dry run. Set DRY_RUN=false to actually cancel."
else
  echo "[cancel-ci-runs] Cancellation requests sent for the runs listed above."
fi
