#!/bin/bash

# Generate a Markdown report from Ralph Wiggum Codex loop logs

set -euo pipefail

ROOT_DIR="$(pwd)"
INDEX_FILE=""
OUTPUT_FILE=""
LIMIT=0

usage() {
  cat <<'EOF'
Generate a Markdown report from Ralph Wiggum Codex loop logs

USAGE:
  scripts/agents/ralph-loop-report.sh [OPTIONS]

OPTIONS:
  --root <dir>       Workspace root (default: pwd)
  --index <path>     Index file (default: .claude/ralph-loop.codex/index.md)
  --output <path>    Report output file (default: .claude/ralph-loop.codex/report.md)
  --limit <n>        Limit to the last N iterations (0 = all)
  -h, --help         Show help
EOF
}

while [[ $# -gt 0 ]]; do
  case $1 in
    -h|--help)
      usage
      exit 0
      ;;
    --root)
      ROOT_DIR="$2"
      shift 2
      ;;
    --index)
      INDEX_FILE="$2"
      shift 2
      ;;
    --output)
      OUTPUT_FILE="$2"
      shift 2
      ;;
    --limit)
      if [[ -z "${2:-}" ]]; then
        echo "Error: --limit requires a number" >&2
        exit 1
      fi
      if ! [[ "$2" =~ ^[0-9]+$ ]]; then
        echo "Error: --limit must be an integer" >&2
        exit 1
      fi
      LIMIT="$2"
      shift 2
      ;;
    *)
      echo "Error: unknown option $1" >&2
      usage
      exit 1
      ;;
  esac
done

ROOT_DIR=$(cd "$ROOT_DIR" && pwd)
if [[ -z "$INDEX_FILE" ]]; then
  INDEX_FILE="$ROOT_DIR/.claude/ralph-loop.codex/index.md"
fi
if [[ -z "$OUTPUT_FILE" ]]; then
  OUTPUT_FILE="$ROOT_DIR/.claude/ralph-loop.codex/report.md"
fi

if [[ ! -f "$INDEX_FILE" ]]; then
  echo "Error: index file not found: $INDEX_FILE" >&2
  exit 1
fi

mkdir -p "$(dirname "$OUTPUT_FILE")"

TMP_TSV="$(mktemp)"
trap 'rm -f "$TMP_TSV"' EXIT

awk '
/^===== Iteration/ {
  iter=$3
  gsub(/[^0-9]/,"",iter)
  next
}
$1=="Started:" {started[iter]=$2; next}
$1=="ExitStatus:" {status[iter]=$2; next}
$1=="LogFile:" {logf[iter]=$2; next}
$1=="LastMessageFile:" {last[iter]=$2; next}
END {
  for (i in started) {
    print i "\t" started[i] "\t" status[i] "\t" logf[i] "\t" last[i]
  }
}
' "$INDEX_FILE" | sort -n -k1,1 > "$TMP_TSV"

if [[ $LIMIT -gt 0 ]]; then
  tail -n "$LIMIT" "$TMP_TSV" > "${TMP_TSV}.tail"
  mv "${TMP_TSV}.tail" "$TMP_TSV"
fi

TOTAL=0
OK=0
FAIL=0

escape_cell() {
  local text="$1"
  text="${text//|/\\|}"
  text="${text//$'\r'/}"
  echo "$text"
}

report_ts="$(date -u +%Y-%m-%dT%H:%M:%SZ)"

{
  echo "# Ralph Loop Report"
  echo
  echo "Generated: $report_ts"
  echo
  echo "- Index: $INDEX_FILE"
  echo "- Output: $OUTPUT_FILE"
  echo
  echo "| Iter | Started (UTC) | Exit | Promise | Outcome | Last Message Preview |"
  echo "| ---- | ------------- | ---- | ------- | ------- | -------------------- |"
} > "$OUTPUT_FILE"

while IFS=$'\t' read -r iter started status log_file last_file; do
  TOTAL=$((TOTAL + 1))
  if [[ "${status:-}" == "0" ]]; then
    OK=$((OK + 1))
    status_label="ok"
  else
    FAIL=$((FAIL + 1))
    status_label="${status:-na}"
  fi

  promise_text=""
  if [[ -f "$last_file" ]]; then
    promise_text=$(perl -0777 -ne 'if (/<promise>(.*?)<\/promise>/s) { $p=$1; $p=~s/^\s+|\s+$//g; $p=~s/\s+/ /g; print $p }' "$last_file" 2>/dev/null || true)
  fi
  if [[ -z "$promise_text" ]]; then
    promise_text="-"
  fi

  outcome_text="-"
  if [[ -f "$last_file" ]]; then
    outcome_text=$(grep -m1 '^Outcome:' "$last_file" 2>/dev/null | sed 's/^Outcome:[[:space:]]*//' || true)
  fi
  if [[ -z "$outcome_text" ]]; then
    outcome_text="-"
  fi

  preview="-"
  if [[ -f "$last_file" ]]; then
    preview=$(grep -m1 -v '^[[:space:]]*$' "$last_file" 2>/dev/null || true)
    if [[ -z "$preview" ]]; then
      preview="-"
    fi
  fi
  preview="$(echo "$preview" | cut -c1-120)"

  printf "| %s | %s | %s | %s | %s | %s |\n" \
    "$(escape_cell "$iter")" \
    "$(escape_cell "${started:-}")" \
    "$(escape_cell "$status_label")" \
    "$(escape_cell "$promise_text")" \
    "$(escape_cell "$outcome_text")" \
    "$(escape_cell "$preview")" \
    >> "$OUTPUT_FILE"
done < "$TMP_TSV"

{
  echo
  echo "Summary: $TOTAL total, $OK ok, $FAIL non-zero."
} >> "$OUTPUT_FILE"

echo "Wrote report to $OUTPUT_FILE"
