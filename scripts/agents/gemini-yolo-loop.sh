#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

PROMPT_FILE=""
PROMPT_DOC=""
SECTION=""
TASK_NAME=""
MAX_ITERATIONS=3
SLEEP_SECONDS=5
MODEL="${GEMINI_MODEL:-}"
TIMEOUT_SECONDS="${GEMINI_TIMEOUT_SECONDS:-900}"
COMPLETION_PROMISE="${GEMINI_COMPLETION_PROMISE:-GEMINI_LOOP_DONE}"
OUTPUT_ROOT="${GEMINI_LOOP_OUTPUT_ROOT:-$ROOT_DIR/.claude/gemini-loop}"
WORKDIR="${GEMINI_LOOP_WORKDIR:-$ROOT_DIR}"
STOP_ON_ERROR=false
DRY_RUN=false
ENABLE_YOLO=true
APPROVAL_MODE="yolo"

INCLUDE_DIRS=()
EXTRA_ARGS=()

usage() {
  cat <<'EOF'
Gemini YOLO loop runner

Usage:
  scripts/agents/gemini-yolo-loop.sh [options]

Prompt source (choose one):
  --prompt-file <path>         Prompt file to run.
  --prompt-doc <path>          Markdown doc that contains task sections.
  --section <heading>          Exact section heading in prompt doc (without leading ##).

Loop control:
  --task-name <name>           Task label for logs/output folder naming.
  --max-iterations <n>         Max attempts before failing (default: 3).
  --sleep-seconds <n>          Delay between attempts (default: 5).
  --timeout-seconds <n>        Kill each iteration after N seconds (default: 900, 0 disables).
  --completion-promise <text>  Completion marker token (default: GEMINI_LOOP_DONE).
  --stop-on-error              Stop immediately if gemini exits non-zero.

Gemini options:
  --model <name>               Gemini model override.
  --approval-mode <mode>       approval mode (default: yolo).
  --no-yolo                    Disable --yolo flag.
  --include-dir <path>         Extra workspace include dir (repeatable).

Execution:
  --workdir <path>             Working directory for gemini execution.
  --output-root <path>         Output root dir for logs/state.
  --dry-run                    Print commands only; do not execute.

Passthrough:
  -- <args...>                 Extra args passed directly to `gemini`.

Examples:
  scripts/agents/gemini-yolo-loop.sh \
    --prompt-doc ~/.context/projects/oracle-of-secrets/scratchpad/gemini_prompts_2026-02-12.md \
    --section "Task 2: Object Dimension Validation" \
    --task-name object-dimension-validation \
    --max-iterations 2 --model gemini-2.5-pro

  scripts/agents/gemini-yolo-loop.sh \
    --prompt-file /tmp/dialogue_prompt.md \
    --task-name dialogue --completion-promise GEMINI_DIALOGUE_DONE \
    -- --sandbox=false
EOF
}

slugify() {
  local raw="$1"
  local lower
  lower="$(printf '%s' "$raw" | tr '[:upper:]' '[:lower:]')"
  printf '%s' "$lower" | sed -E 's/[^a-z0-9]+/-/g; s/^-+//; s/-+$//; s/-{2,}/-/g'
}

extract_section() {
  local doc="$1"
  local section="$2"
  local out_file="$3"

  if [[ ! -f "$doc" ]]; then
    echo "prompt doc not found: $doc" >&2
    return 1
  fi

  if ! awk -v section="$section" '
BEGIN { in_section = 0; found = 0 }
{
  if ($0 ~ /^## /) {
    if (in_section && $0 != ("## " section)) {
      exit
    }
    if ($0 == ("## " section)) {
      in_section = 1
      found = 1
      next
    }
  }
  if (in_section) {
    print
  }
}
END {
  if (!found) {
    exit 3
  }
}
' "$doc" > "$out_file"; then
    echo "failed to extract section: $section" >&2
    return 1
  fi

  if [[ ! -s "$out_file" ]]; then
    echo "section was found but empty: $section" >&2
    return 1
  fi
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --prompt-file)
      PROMPT_FILE="$2"
      shift 2
      ;;
    --prompt-doc)
      PROMPT_DOC="$2"
      shift 2
      ;;
    --section)
      SECTION="$2"
      shift 2
      ;;
    --task-name)
      TASK_NAME="$2"
      shift 2
      ;;
    --max-iterations)
      MAX_ITERATIONS="$2"
      shift 2
      ;;
    --sleep-seconds)
      SLEEP_SECONDS="$2"
      shift 2
      ;;
    --timeout-seconds)
      TIMEOUT_SECONDS="$2"
      shift 2
      ;;
    --completion-promise)
      COMPLETION_PROMISE="$2"
      shift 2
      ;;
    --model)
      MODEL="$2"
      shift 2
      ;;
    --approval-mode)
      APPROVAL_MODE="$2"
      shift 2
      ;;
    --no-yolo)
      ENABLE_YOLO=false
      shift
      ;;
    --include-dir)
      INCLUDE_DIRS+=("$2")
      shift 2
      ;;
    --workdir)
      WORKDIR="$2"
      shift 2
      ;;
    --output-root)
      OUTPUT_ROOT="$2"
      shift 2
      ;;
    --stop-on-error)
      STOP_ON_ERROR=true
      shift
      ;;
    --dry-run)
      DRY_RUN=true
      shift
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    --)
      shift
      EXTRA_ARGS+=("$@")
      break
      ;;
    -* )
      echo "unknown option: $1" >&2
      usage
      exit 1
      ;;
    *)
      if [[ -z "$PROMPT_FILE" && -z "$PROMPT_DOC" ]]; then
        PROMPT_FILE="$1"
        shift
      else
        echo "unexpected positional argument: $1" >&2
        usage
        exit 1
      fi
      ;;
  esac
done

if [[ -n "$PROMPT_FILE" && -n "$PROMPT_DOC" ]]; then
  echo "choose one prompt source: --prompt-file or --prompt-doc/--section" >&2
  exit 1
fi

if [[ -z "$PROMPT_FILE" ]]; then
  if [[ -z "$PROMPT_DOC" || -z "$SECTION" ]]; then
    echo "when using --prompt-doc you must also provide --section" >&2
    exit 1
  fi
else
  if [[ ! -f "$PROMPT_FILE" ]]; then
    echo "prompt file not found: $PROMPT_FILE" >&2
    exit 1
  fi
fi

if [[ -z "$TASK_NAME" ]]; then
  if [[ -n "$SECTION" ]]; then
    TASK_NAME="$SECTION"
  else
    TASK_NAME="$(basename "$PROMPT_FILE")"
  fi
fi

if ! [[ "$MAX_ITERATIONS" =~ ^[0-9]+$ ]] || [[ "$MAX_ITERATIONS" -lt 1 ]]; then
  echo "--max-iterations must be >= 1" >&2
  exit 1
fi

if ! [[ "$SLEEP_SECONDS" =~ ^[0-9]+$ ]]; then
  echo "--sleep-seconds must be >= 0" >&2
  exit 1
fi

if ! [[ "$TIMEOUT_SECONDS" =~ ^[0-9]+$ ]]; then
  echo "--timeout-seconds must be >= 0" >&2
  exit 1
fi

if [[ ! -d "$WORKDIR" ]]; then
  echo "workdir does not exist: $WORKDIR" >&2
  exit 1
fi

if ! command -v gemini >/dev/null 2>&1; then
  echo "gemini CLI not found in PATH" >&2
  exit 1
fi

TASK_SLUG="$(slugify "$TASK_NAME")"
if [[ -z "$TASK_SLUG" ]]; then
  TASK_SLUG="task"
fi

RUN_ID="$(date +%Y%m%d-%H%M%S)"
RUN_DIR="$OUTPUT_ROOT/${TASK_SLUG}-${RUN_ID}"
mkdir -p "$RUN_DIR"

BASE_PROMPT_FILE="$RUN_DIR/base.prompt.md"
if [[ -n "$PROMPT_FILE" ]]; then
  cp "$PROMPT_FILE" "$BASE_PROMPT_FILE"
else
  extract_section "$PROMPT_DOC" "$SECTION" "$BASE_PROMPT_FILE"
fi

COMPLETION_MARKER="<promise>${COMPLETION_PROMISE}</promise>"

build_prompt_for_iteration() {
  local prompt_out="$1"
  local previous_log="${2:-}"

  cat "$BASE_PROMPT_FILE" > "$prompt_out"
  cat >> "$prompt_out" <<EOF

---

Loop execution contract:
- Write all requested artifacts directly to disk.
- If the task is complete, include this exact marker on its own line:
$COMPLETION_MARKER
- If blocked, explain the blocker and do not emit the marker.
EOF

  if [[ -n "$previous_log" && -f "$previous_log" ]]; then
    cat >> "$prompt_out" <<EOF

Previous attempt output tail (for self-correction):
EOF
    tail -n 120 "$previous_log" >> "$prompt_out" || true
  fi
}

build_gemini_cmd() {
  local -n _cmd_ref=$1
  _cmd_ref=(gemini --output-format text)

  if [[ -n "$APPROVAL_MODE" ]]; then
    _cmd_ref+=(--approval-mode "$APPROVAL_MODE")
  elif [[ "$ENABLE_YOLO" == true ]]; then
    # Legacy fallback for older CLI versions that use only --yolo.
    _cmd_ref+=(--yolo)
  fi

  if [[ -n "$MODEL" ]]; then
    _cmd_ref+=(--model "$MODEL")
  fi

  for include_dir in "${INCLUDE_DIRS[@]}"; do
    _cmd_ref+=(--include-directories "$include_dir")
  done

  if [[ ${#EXTRA_ARGS[@]} -gt 0 ]]; then
    _cmd_ref+=("${EXTRA_ARGS[@]}")
  fi

  _cmd_ref+=(
    -p
    "Execute all instructions from stdin. Only emit ${COMPLETION_MARKER} when fully complete."
  )
}

write_run_summary() {
  local completed="$1"
  local attempts="$2"
  local last_status="$3"
  local summary_file="$RUN_DIR/run-summary.md"

  cat > "$summary_file" <<EOF
# Gemini YOLO Loop Run

- TaskName: $TASK_NAME
- RunDir: $RUN_DIR
- Workdir: $WORKDIR
- Completed: $completed
- Attempts: $attempts
- LastExitStatus: $last_status
- TimeoutSeconds: $TIMEOUT_SECONDS
- CompletionMarker: $COMPLETION_MARKER

## Files
- base.prompt.md
- iteration-*.prompt.md
- iteration-*.log
- run-summary.md
EOF
}

LAST_LOG=""
LAST_STATUS=0
COMPLETED=false
ATTEMPTS=0

for ((ITER=1; ITER<=MAX_ITERATIONS; ITER++)); do
  ATTEMPTS="$ITER"
  ITER_TAG="$(printf '%03d' "$ITER")"
  ITER_PROMPT="$RUN_DIR/iteration-${ITER_TAG}.prompt.md"
  ITER_LOG="$RUN_DIR/iteration-${ITER_TAG}.log"

  build_prompt_for_iteration "$ITER_PROMPT" "$LAST_LOG"

  GEMINI_CMD=()
  build_gemini_cmd GEMINI_CMD

  echo "[gemini-loop] iteration $ITER/$MAX_ITERATIONS"
  echo "[gemini-loop] run dir: $RUN_DIR"

  if [[ "$DRY_RUN" == true ]]; then
    {
      printf 'DRY RUN command:'
      for token in "${GEMINI_CMD[@]}"; do
        printf ' %q' "$token"
      done
      printf '\n'
      printf 'DRY RUN prompt: %s\n' "$ITER_PROMPT"
      printf 'DRY RUN workdir: %s\n' "$WORKDIR"
    } | tee "$ITER_LOG"
    LAST_LOG="$ITER_LOG"
    LAST_STATUS=0
    continue
  fi

  set +e
  if [[ "$TIMEOUT_SECONDS" -gt 0 ]]; then
    (
      cd "$WORKDIR"
      timeout --signal=TERM --kill-after=10s "${TIMEOUT_SECONDS}s" "${GEMINI_CMD[@]}" < "$ITER_PROMPT"
    ) > "$ITER_LOG" 2>&1
  else
    (
      cd "$WORKDIR"
      "${GEMINI_CMD[@]}" < "$ITER_PROMPT"
    ) > "$ITER_LOG" 2>&1
  fi
  LAST_STATUS=$?
  set -e

  LAST_LOG="$ITER_LOG"

  if grep -Fq "$COMPLETION_MARKER" "$ITER_LOG"; then
    COMPLETED=true
    break
  fi

  if [[ $LAST_STATUS -ne 0 && "$STOP_ON_ERROR" == true ]]; then
    break
  fi

  if [[ $LAST_STATUS -eq 124 ]]; then
    echo "[gemini-loop] iteration $ITER timed out after ${TIMEOUT_SECONDS}s" >&2
  fi

  if [[ "$ITER" -lt "$MAX_ITERATIONS" && "$SLEEP_SECONDS" -gt 0 ]]; then
    sleep "$SLEEP_SECONDS"
  fi
done

write_run_summary "$COMPLETED" "$ATTEMPTS" "$LAST_STATUS"

echo "[gemini-loop] summary: $RUN_DIR/run-summary.md"

if [[ "$COMPLETED" == true ]]; then
  echo "[gemini-loop] completed with marker: $COMPLETION_MARKER"
  exit 0
fi

if [[ "$DRY_RUN" == true ]]; then
  echo "[gemini-loop] dry-run complete"
  exit 0
fi

echo "[gemini-loop] completion marker not found after $ATTEMPTS iteration(s)" >&2
exit 2
