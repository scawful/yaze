#!/usr/bin/env bash
set -euo pipefail

# Enforces editor quality guardrails on changed code between two git refs.
# Usage:
#   scripts/dev/editor-guardrails.sh <base-ref> <head-ref>

if [[ $# -ne 2 ]]; then
  echo "Usage: $0 <base-ref> <head-ref>" >&2
  exit 2
fi

base_ref="$1"
head_ref="$2"

if ! git rev-parse --verify "$base_ref" >/dev/null 2>&1; then
  echo "Base ref not found: $base_ref" >&2
  exit 2
fi
if ! git rev-parse --verify "$head_ref" >/dev/null 2>&1; then
  echo "Head ref not found: $head_ref" >&2
  exit 2
fi

failures=()

mapfile -t new_files < <(git diff --name-status "$base_ref" "$head_ref" | awk '$1=="A"{print $2}')

# Allowed parent directories for new *_panel.{cc,h} files. Anything outside these
# is treated as an ad-hoc panel file and rejected. Keep this list in sync with
# docs/internal/plans/0.7.1-release-plan.md (Theme 0: editor source layout).
panel_allowed_prefixes=(
  "src/app/editor/shell/windows/"
  "src/app/editor/hack/oracle/ui/"
  "src/app/editor/hack/workflow/"
  "src/app/editor/system/workspace/"
  "src/app/editor/code/"
)

for file in "${new_files[@]}"; do
  # Explicit exemption: overworld_panel_access.h is an intentional bridge header
  # kept at the legacy path; also exempted from the dynamic_cast check below.
  if [[ "$file" == "src/app/editor/overworld/panels/overworld_panel_access.h" ]]; then
    continue
  fi
  if [[ "$file" =~ ^src/app/editor/.*/panels/.*\.(cc|h)$ ]]; then
    failures+=("new panels/ file is disallowed: $file")
  fi
  if [[ "$file" =~ _panel\.(cc|h)$ ]]; then
    # Feature-module WindowContent shells under */ui/window/ are always allowed.
    if [[ "$file" =~ /ui/window/ ]]; then
      continue
    fi
    allowed=false
    for prefix in "${panel_allowed_prefixes[@]}"; do
      if [[ "$file" == "$prefix"* ]]; then
        allowed=true
        break
      fi
    done
    if [[ "$allowed" == false ]]; then
      failures+=("new *_panel file is disallowed: $file")
    fi
  fi
done

mapfile -t changed_cpp < <(git diff --name-only "$base_ref" "$head_ref" -- 'src/app/editor/**/*.cc' 'src/app/editor/**/*.h')
for file in "${changed_cpp[@]}"; do
  if [[ -z "$file" || ! -f "$file" ]]; then
    continue
  fi
  if [[ "$file" == "src/app/editor/overworld/panels/overworld_panel_access.h" ]]; then
    continue
  fi

  # Guardrail: avoid introducing new concrete-editor downcasts in editor files.
  if git diff -U0 "$base_ref" "$head_ref" -- "$file" | rg '^\+.*dynamic_cast<.*Editor\*>' -q; then
    failures+=("new concrete dynamic_cast added in $file (use typed accessor/interface)")
  fi
done

mapfile -t changed_editor_cc < <(git diff --numstat "$base_ref" "$head_ref" -- 'src/app/editor/**/*.cc' | awk '{print $1":"$2":"$3}')
for entry in "${changed_editor_cc[@]}"; do
  added="${entry%%:*}"
  rest="${entry#*:}"
  deleted="${rest%%:*}"
  file="${rest#*:}"

  [[ -z "$file" || ! -f "$file" ]] && continue
  [[ "$added" == "-" ]] && continue
  [[ "$deleted" == "-" ]] && deleted=0

  # Use net growth: if a refactor removes more than it adds, the file is
  # shrinking even if individual chunks are large.
  net_growth=$((added - deleted))
  total_lines="$(wc -l < "$file" | tr -d ' ')"
  if (( total_lines > 3000 && net_growth > 50 )); then
    failures+=("mega-file growth blocked ($file: ${total_lines} lines, +${net_growth} net lines)")
  fi
done

if (( ${#failures[@]} > 0 )); then
  echo "Editor guardrails failed:"
  for msg in "${failures[@]}"; do
    echo " - $msg"
  done
  exit 1
fi

echo "Editor guardrails passed."
