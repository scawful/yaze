#!/usr/bin/env bash
# Guard source occurrences for common ImGui color debt patterns.
# This is a textual budget, not a semantic classification of every color.
# Baselines measured on the integration branch after the Cursor second pass.
# Baselines are the current debt. Lower them after a migration; do not raise
# them to admit new literals. Theme tokens and StyleColorGuard are the path.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

if ! command -v rg >/dev/null 2>&1; then
  echo "error: ripgrep (rg) is required" >&2
  exit 1
fi

# pattern|baseline
CHECKS=(
  "ImVec4(|417"
  "IM_COL32(|133"
  "ImGui::PushStyleColor|0"
)

count_pattern() {
  local pattern="$1"
  local matches status
  if matches="$(rg -F --count-matches "$pattern" src/app/editor --glob '*.cc' --glob '*.h')"; then
    printf '%s\n' "$matches" | awk -F: '{s+=$NF} END {print s+0}'
  else
    status=$?
    if [[ "$status" -eq 1 ]]; then
      printf '0\n'
    else
      return "$status"
    fi
  fi
}

failed=0
for entry in "${CHECKS[@]}"; do
  pattern="${entry%%|*}"
  baseline="${entry##*|}"
  count="$(count_pattern "$pattern")"
  if [[ "$count" -gt "$baseline" ]]; then
    echo "imgui-literal-audit: ${pattern} count ${count} exceeds baseline ${baseline}"
    rg -F --count-matches "$pattern" src/app/editor --glob '*.cc' --glob '*.h' \
      | sort -t: -k2 -nr | awk 'NR <= 15'
    failed=1
  else
    echo "imgui-literal-audit: ${pattern} ${count} (baseline ${baseline})"
  fi
done

exit "$failed"
