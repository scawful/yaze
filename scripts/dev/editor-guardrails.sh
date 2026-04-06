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
for file in "${new_files[@]}"; do
  if [[ "$file" =~ ^src/app/editor/.*/panels/.*\.(cc|h)$ ]]; then
    failures+=("new panels/ file is disallowed: $file")
  fi
  if [[ "$file" =~ _panel\.(cc|h)$ ]]; then
    failures+=("new *_panel file is disallowed: $file")
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
  _deleted="${rest%%:*}"
  file="${rest#*:}"

  [[ -z "$file" || ! -f "$file" ]] && continue
  [[ "$added" == "-" ]] && continue

  total_lines="$(wc -l < "$file" | tr -d ' ')"
  if (( total_lines > 3000 && added > 50 )); then
    failures+=("mega-file growth blocked ($file: ${total_lines} lines, +${added} lines)")
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
