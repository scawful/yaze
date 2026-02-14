#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOOP_SCRIPT="$SCRIPT_DIR/gemini-yolo-loop.sh"

PROMPT_DOC="${GEMINI_PROMPT_DOC:-$HOME/.context/projects/oracle-of-secrets/scratchpad/gemini_prompts_2026-02-12.md}"
OUTPUT_ROOT="${GEMINI_ORACLE_OUTPUT_ROOT:-$HOME/.context/projects/oracle-of-secrets/scratchpad/gemini-yolo-runs}"
WORKDIR="${GEMINI_ORACLE_WORKDIR:-$HOME/src/hobby/oracle-of-secrets}"
MODEL="${GEMINI_MODEL:-}"
MAX_ITERATIONS=3
TIMEOUT_SECONDS="${GEMINI_TIMEOUT_SECONDS:-900}"
TASKS_CSV="dialogue,dimensions,wrap,annotation"
PARALLEL=false
DRY_RUN=false

INCLUDE_DIRS=(
  "$HOME/src/hobby/oracle-of-secrets"
  "$HOME/src/hobby/yaze"
  "$HOME/src/hobby/ZScreamDungeon"
  "$HOME/.context/projects/oracle-of-secrets/scratchpad"
)
EXTRA_ARGS=()

usage() {
  cat <<'EOF'
Run Oracle workstream prompts through Gemini YOLO loops.

Usage:
  scripts/agents/gemini-oracle-workstream.sh [options] [-- <extra gemini args>]

Options:
  --tasks <csv>            Task keys: dialogue,dimensions,wrap,annotation,all
                           (default: dialogue,dimensions,wrap,annotation)
  --prompt-doc <path>      Prompt markdown doc path.
  --output-root <path>     Output root for per-task loop logs.
  --workdir <path>         Working directory for gemini execution.
  --model <name>           Gemini model override.
  --max-iterations <n>     Max loop attempts per task (default: 3).
  --timeout-seconds <n>    Per-iteration timeout passed to loop (default: 900).
  --parallel               Run selected tasks in parallel.
  --include-dir <path>     Add include directory (repeatable).
  --dry-run                Print loop commands only.
  --help                   Show help.

Examples:
  # Run all four tasks sequentially with YOLO mode
  scripts/agents/gemini-oracle-workstream.sh --model gemini-2.5-pro --max-iterations 2

  # Run only dialogue + wrap in parallel
  scripts/agents/gemini-oracle-workstream.sh --tasks dialogue,wrap --parallel

  # Pass extra gemini args
  scripts/agents/gemini-oracle-workstream.sh -- --sandbox=false
EOF
}

normalize_task() {
  local task="$1"
  case "$task" in
    dialogue|dimensions|wrap|annotation)
      printf '%s' "$task"
      ;;
    all)
      printf '%s' "dialogue dimensions wrap annotation"
      ;;
    *)
      echo "unknown task key: $task" >&2
      return 1
      ;;
  esac
}

section_for_task() {
  local task="$1"
  case "$task" in
    dialogue)
      printf '%s' "Task 1: NPC Dialogue Bundles (50+ messages)"
      ;;
    dimensions)
      printf '%s' "Task 2: Object Dimension Validation"
      ;;
    wrap)
      printf '%s' "Task 3: 32-Character Wrap Validation Pass"
      ;;
    annotation)
      printf '%s' "Task 4: Annotation Overlay Design Spec"
      ;;
    *)
      echo "unknown task key: $task" >&2
      return 1
      ;;
  esac
}

promise_for_task() {
  local task="$1"
  case "$task" in
    dialogue)
      printf '%s' "GEMINI_DIALOGUE_DONE"
      ;;
    dimensions)
      printf '%s' "GEMINI_DIMENSIONS_DONE"
      ;;
    wrap)
      printf '%s' "GEMINI_WRAP_DONE"
      ;;
    annotation)
      printf '%s' "GEMINI_ANNOTATION_DONE"
      ;;
    *)
      echo "unknown task key: $task" >&2
      return 1
      ;;
  esac
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --tasks)
      TASKS_CSV="$2"
      shift 2
      ;;
    --prompt-doc)
      PROMPT_DOC="$2"
      shift 2
      ;;
    --output-root)
      OUTPUT_ROOT="$2"
      shift 2
      ;;
    --workdir)
      WORKDIR="$2"
      shift 2
      ;;
    --model)
      MODEL="$2"
      shift 2
      ;;
    --max-iterations)
      MAX_ITERATIONS="$2"
      shift 2
      ;;
    --timeout-seconds)
      TIMEOUT_SECONDS="$2"
      shift 2
      ;;
    --parallel)
      PARALLEL=true
      shift
      ;;
    --include-dir)
      INCLUDE_DIRS+=("$2")
      shift 2
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
      echo "unexpected positional argument: $1" >&2
      usage
      exit 1
      ;;
  esac
done

if [[ ! -x "$LOOP_SCRIPT" ]]; then
  echo "loop runner not executable: $LOOP_SCRIPT" >&2
  exit 1
fi

if [[ ! -f "$PROMPT_DOC" ]]; then
  echo "prompt doc not found: $PROMPT_DOC" >&2
  exit 1
fi

if ! [[ "$MAX_ITERATIONS" =~ ^[0-9]+$ ]] || [[ "$MAX_ITERATIONS" -lt 1 ]]; then
  echo "--max-iterations must be >= 1" >&2
  exit 1
fi

if ! [[ "$TIMEOUT_SECONDS" =~ ^[0-9]+$ ]]; then
  echo "--timeout-seconds must be >= 0" >&2
  exit 1
fi

IFS=',' read -r -a TASK_ITEMS <<< "$TASKS_CSV"
SELECTED_TASKS=()
for raw_task in "${TASK_ITEMS[@]}"; do
  task="$(echo "$raw_task" | xargs)"
  if [[ -z "$task" ]]; then
    continue
  fi

  normalized="$(normalize_task "$task")"
  if [[ "$normalized" == "dialogue dimensions wrap annotation" ]]; then
    SELECTED_TASKS=(dialogue dimensions wrap annotation)
    break
  fi

  SELECTED_TASKS+=("$normalized")
done

if [[ ${#SELECTED_TASKS[@]} -eq 0 ]]; then
  echo "no tasks selected" >&2
  exit 1
fi

run_task() {
  local task="$1"
  local section
  section="$(section_for_task "$task")"
  local promise
  promise="$(promise_for_task "$task")"

  local task_output_root="$OUTPUT_ROOT/$task"
  mkdir -p "$task_output_root"

  local cmd=(
    "$LOOP_SCRIPT"
    --prompt-doc "$PROMPT_DOC"
    --section "$section"
    --task-name "$task"
    --completion-promise "$promise"
    --max-iterations "$MAX_ITERATIONS"
    --timeout-seconds "$TIMEOUT_SECONDS"
    --workdir "$WORKDIR"
    --output-root "$task_output_root"
  )

  if [[ -n "$MODEL" ]]; then
    cmd+=(--model "$MODEL")
  fi

  for include_dir in "${INCLUDE_DIRS[@]}"; do
    cmd+=(--include-dir "$include_dir")
  done

  if [[ "$DRY_RUN" == true ]]; then
    cmd+=(--dry-run)
  fi

  if [[ ${#EXTRA_ARGS[@]} -gt 0 ]]; then
    cmd+=(-- "${EXTRA_ARGS[@]}")
  fi

  echo "[gemini-oracle] starting task: $task"
  "${cmd[@]}"
}

if [[ "$PARALLEL" == true ]]; then
  declare -A PIDS=()
  FAIL=0

  for task in "${SELECTED_TASKS[@]}"; do
    (
      run_task "$task"
    ) &
    PIDS["$task"]=$!
  done

  for task in "${SELECTED_TASKS[@]}"; do
    pid="${PIDS[$task]}"
    if ! wait "$pid"; then
      echo "[gemini-oracle] task failed: $task" >&2
      FAIL=1
    else
      echo "[gemini-oracle] task complete: $task"
    fi
  done

  exit "$FAIL"
fi

for task in "${SELECTED_TASKS[@]}"; do
  run_task "$task"
  echo "[gemini-oracle] task complete: $task"
done
