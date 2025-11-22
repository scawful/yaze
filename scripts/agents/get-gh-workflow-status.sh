#!/bin/bash
#
# A script to check the status of a GitHub Actions workflow run.
#
# Usage: ./get-gh-workflow-status.sh <run_url>
#
# Requires `gh` (GitHub CLI) and `jq` to be installed and authenticated.

set -euo pipefail

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <run_url_or_run_id>"
    exit 1
fi

RUN_ID_OR_URL="$1"

# Extract run ID from URL if a URL is provided
if [[ "$RUN_ID_OR_URL" == *"github.com"* ]]; then
    RUN_ID=$(basename "$RUN_ID_OR_URL")
else
    RUN_ID="$RUN_ID_OR_URL"
fi

echo "Fetching status for workflow run ID: $RUN_ID..."

# Use GitHub CLI to get the run and its jobs, then format with jq
gh run view "$RUN_ID" --json jobs,status,conclusion,name,url --jq '
    "Run: " + .name + " (" + .status + "/" + (.conclusion // "in_progress") + ")",
    "URL: " + .url,
    "",
    "Jobs:",
    "----",
    (.jobs[] | "  - " + .name + ": " + .conclusion + " (" + (.status // "unknown") + ")")
'
