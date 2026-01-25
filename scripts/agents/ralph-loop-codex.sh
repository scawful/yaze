#!/bin/bash

# Ralph Wiggum loop runner for Codex CLI
# Runs codex exec repeatedly with the same prompt until a promise is met

set -euo pipefail

PROMPT_PARTS=()
PROMPT_FILE=""
ROOT_DIR="$(pwd)"
STATE_FILE=".claude/ralph-loop.codex.md"
OUTPUT_DIR=".claude/ralph-loop.codex"
MAX_ITERATIONS=0
COMPLETION_PROMISE=""
STOP_ON_ERROR=0
STREAM_OUTPUT=1
TAIL_LINES=0
PRINT_LAST_LINES=40
INDEX_FILE=""
COMBINED_LOG_FILE=""
AUTO_EXTEND=0
EXTEND_BY=20
REPORT_EVERY=0
REPORT_LIMIT=20
REPORT_SCRIPT=""
HEARTBEAT_SECONDS=30
HEARTBEAT_FILE=""
HEARTBEAT_PID=""
WATCHDOG_SECONDS=300
WATCHDOG_RETRIES=1
WATCHDOG_FILE=""
WATCHDOG_PID=""
ACTIVITY_FILE=""
REPO_STATUS_FILE=""
GUARDRAILS_FILE=""
INJECT_GUARDRAILS=1
USE_COLOR=1
PREFIX_OUTPUT=1
PREFIX_TAG="codex"
PREFERENCES_FILE="${RALPH_LOOP_PREFS:-$HOME/.config/ralph-loop/preferences.md}"
SANITY_ENABLED=1
SANITY_EVERY=1
SANITY_SCRIPT=""
SANITY_STRICT=0
PREFLIGHT_ENABLED=1
PREFLIGHT_EVERY=1
PREFLIGHT_SCRIPT=""
PREFLIGHT_STRICT=0
PREFLIGHT_REBUILD_DIRTY=0
CODEX_ARGS=()
MESEN_REGISTRY_INSTANCE=""
MESEN_REGISTRY_OWNER=""
MESEN_REGISTRY_ROM=""
MESEN_REGISTRY_SCRIPT=""
MESEN_REGISTRY_SOCKET=""
MESEN_REGISTRY_PID=""
RESOLVED_SOCKET=""
RESOLVED_SOCKET_SOURCE=""
RESOLVED_SOCKET_AT=""
OOS_ROOT="${RALPH_OOS_ROOT:-$HOME/src/hobby/oracle-of-secrets}"
SOCKET_RECOVERY_STATUS="not_run"
SOCKET_RECOVERY_SOCKET=""
SOCKET_RECOVERY_AT=""
SANITY_RETRY_NEEDED=0
LAST_PREFLIGHT_STATUS="not_run"
LAST_PREFLIGHT_CODE="null"
LAST_SANITY_STATUS="not_run"
LAST_SANITY_CODE="null"
LAST_EXIT_STATUS="null"
LAST_OUTCOME_STATUS="unknown"
LAST_MICRO_TASK_STATUS="unknown"
LAST_MESSAGE_EMPTY="unknown"
WATCHDOG_TRIGGERED=0
STATE_BODY=""
STATE_WRITTEN=0
STATE_ACTIVE=0
STATE_ENDED_AT=""

usage() {
  cat <<'EOF'
Ralph Wiggum loop for Codex CLI

USAGE:
  scripts/agents/ralph-loop-codex.sh [PROMPT...] [OPTIONS] [-- <codex exec args...>]

ARGUMENTS:
  PROMPT...                 Prompt text (if --prompt-file not used)

OPTIONS:
  --prompt-file <path>      Read prompt text from file
  --mission <path>          Alias for --prompt-file
  --root <dir>              Working directory for codex exec (default: pwd)
  --state-file <path>       State file path (default: .claude/ralph-loop.codex.md)
  --output-dir <path>       Output directory for iteration logs
  --max-iterations <n>      Maximum iterations before stop (0 = unlimited)
  --completion-promise <t>  Promise phrase used inside <promise> tags
  --stop-on-error           Stop if codex exec returns non-zero
  --no-stream               Do not stream codex output to the console
  --tail-lines <n>          Print the last N log lines after each iteration
  --print-last-lines <n>    Print the first N lines of the final agent message (default: 40)
  --auto-extend             Increase max iterations when promise not met
  --extend-by <n>           How many iterations to extend by (default: 20)
  --report-every <n>        Generate report every N iterations (0 = off)
  --report-limit <n>        Limit report to the last N iterations (default: 20)
  --report-script <path>    Report script path (default: scripts/agents/ralph-loop-report.sh)
  --heartbeat-seconds <n>   Write a heartbeat to the log every N seconds (0 = off)
  --watchdog-seconds <n>    Restart iteration if no new output for N seconds (default: 300; 0 = off)
  --watchdog-retries <n>    Restart attempts when watchdog triggers (default: 1)
  --guardrails-file <path>  Append guardrails text to the prompt each iteration
  --mesen-instance <name>   Claim/heartbeat Mesen2 registry instance (optional)
  --mesen-owner <name>      Owner label for Mesen2 registry (optional)
  --mesen-rom <path>        ROM path to match when claiming (optional)
  --mesen-socket <path>     Socket path to claim (preferred for multi-instance)
  --mesen-pid <pid>         PID to claim (alternative to socket)
  --mesen-registry <path>   Override path to mesen2_registry.py (optional; default: $RALPH_OOS_ROOT/scripts/mesen2_registry.py)
  --sanity-script <path>    Sanity check script (default: $RALPH_OOS_ROOT/scripts/mesen2_sanity_check.sh)
  --sanity-every <n>        Run sanity check every N iterations (default: 1)
  --sanity-strict           Fail sanity check on warnings
  --no-sanity               Disable sanity checks
  --preflight-script <path> Preflight script (default: $RALPH_OOS_ROOT/scripts/mesen2_preflight.sh)
  --preflight-every <n>     Run preflight every N iterations (default: 1)
  --preflight-strict        Fail preflight on warnings
  --preflight-rebuild-dirty Rebuild Mesen2 when repo is dirty
  --no-preflight            Disable preflight
  --no-guardrails           Disable automatic guardrails injection
  --no-color                Disable ANSI color output
  --color                   Force ANSI color output
  --no-prefix-output        Disable line prefixing for codex output
  --prefix-tag <name>       Prefix tag for output lines (default: codex)
  -h, --help                Show help

PASS-THROUGH:
  Any args after "--" are passed to `codex exec` verbatim.
  Example:
    scripts/agents/ralph-loop-codex.sh --mission docs/internal/plans/ralph-wiggum-codex-mission.md \
      --max-iterations 15 --completion-promise "YAZE_RALPH_DONE" \
      -- --profile mac-ai --full-auto

NOTES:
  - Default codex args include --full-auto if none are provided.
  - State files are stored under .claude/ (gitignored).
  - RALPH_OOS_ROOT can override the Oracle-of-Secrets root used for sanity/preflight defaults.
EOF
}

while [[ $# -gt 0 ]]; do
  case $1 in
    -h|--help)
      usage
      exit 0
      ;;
    --prompt-file|--mission)
      PROMPT_FILE="$2"
      shift 2
      ;;
    --root|--workspace|--cwd|-C)
      ROOT_DIR="$2"
      shift 2
      ;;
    --state-file)
      STATE_FILE="$2"
      shift 2
      ;;
    --output-dir)
      OUTPUT_DIR="$2"
      shift 2
      ;;
    --max-iterations)
      if [[ -z "${2:-}" ]]; then
        echo "Error: --max-iterations requires a number" >&2
        exit 1
      fi
      if ! [[ "$2" =~ ^[0-9]+$ ]]; then
        echo "Error: --max-iterations must be an integer" >&2
        exit 1
      fi
      MAX_ITERATIONS="$2"
      shift 2
      ;;
    --completion-promise)
      if [[ -z "${2:-}" ]]; then
        echo "Error: --completion-promise requires text" >&2
        exit 1
      fi
      COMPLETION_PROMISE="$2"
      shift 2
      ;;
    --stop-on-error)
      STOP_ON_ERROR=1
      shift
      ;;
    --no-stream)
      STREAM_OUTPUT=0
      shift
      ;;
    --tail-lines)
      if [[ -z "${2:-}" ]]; then
        echo "Error: --tail-lines requires a number" >&2
        exit 1
      fi
      if ! [[ "$2" =~ ^[0-9]+$ ]]; then
        echo "Error: --tail-lines must be an integer" >&2
        exit 1
      fi
      TAIL_LINES="$2"
      shift 2
      ;;
    --print-last-lines)
      if [[ -z "${2:-}" ]]; then
        echo "Error: --print-last-lines requires a number" >&2
        exit 1
      fi
      if ! [[ "$2" =~ ^[0-9]+$ ]]; then
        echo "Error: --print-last-lines must be an integer" >&2
        exit 1
      fi
      PRINT_LAST_LINES="$2"
      shift 2
      ;;
    --auto-extend)
      AUTO_EXTEND=1
      shift
      ;;
    --extend-by)
      if [[ -z "${2:-}" ]]; then
        echo "Error: --extend-by requires a number" >&2
        exit 1
      fi
      if ! [[ "$2" =~ ^[0-9]+$ ]]; then
        echo "Error: --extend-by must be an integer" >&2
        exit 1
      fi
      EXTEND_BY="$2"
      shift 2
      ;;
    --report-every)
      if [[ -z "${2:-}" ]]; then
        echo "Error: --report-every requires a number" >&2
        exit 1
      fi
      if ! [[ "$2" =~ ^[0-9]+$ ]]; then
        echo "Error: --report-every must be an integer" >&2
        exit 1
      fi
      REPORT_EVERY="$2"
      shift 2
      ;;
    --report-limit)
      if [[ -z "${2:-}" ]]; then
        echo "Error: --report-limit requires a number" >&2
        exit 1
      fi
      if ! [[ "$2" =~ ^[0-9]+$ ]]; then
        echo "Error: --report-limit must be an integer" >&2
        exit 1
      fi
      REPORT_LIMIT="$2"
      shift 2
      ;;
    --report-script)
      REPORT_SCRIPT="$2"
      shift 2
      ;;
    --heartbeat-seconds)
      if [[ -z "${2:-}" ]]; then
        echo "Error: --heartbeat-seconds requires a number" >&2
        exit 1
      fi
      if ! [[ "$2" =~ ^[0-9]+$ ]]; then
        echo "Error: --heartbeat-seconds must be an integer" >&2
        exit 1
      fi
      HEARTBEAT_SECONDS="$2"
      shift 2
      ;;
    --watchdog-seconds)
      if [[ -z "${2:-}" ]]; then
        echo "Error: --watchdog-seconds requires a number" >&2
        exit 1
      fi
      if ! [[ "$2" =~ ^[0-9]+$ ]]; then
        echo "Error: --watchdog-seconds must be an integer" >&2
        exit 1
      fi
      WATCHDOG_SECONDS="$2"
      shift 2
      ;;
    --watchdog-retries)
      if [[ -z "${2:-}" ]]; then
        echo "Error: --watchdog-retries requires a number" >&2
        exit 1
      fi
      if ! [[ "$2" =~ ^[0-9]+$ ]]; then
        echo "Error: --watchdog-retries must be an integer" >&2
        exit 1
      fi
      WATCHDOG_RETRIES="$2"
      shift 2
      ;;
    --guardrails-file)
      GUARDRAILS_FILE="$2"
      shift 2
      ;;
    --mesen-instance)
      MESEN_REGISTRY_INSTANCE="$2"
      shift 2
      ;;
    --mesen-owner)
      MESEN_REGISTRY_OWNER="$2"
      shift 2
      ;;
    --mesen-rom)
      MESEN_REGISTRY_ROM="$2"
      shift 2
      ;;
    --mesen-socket)
      MESEN_REGISTRY_SOCKET="$2"
      shift 2
      ;;
    --mesen-pid)
      MESEN_REGISTRY_PID="$2"
      shift 2
      ;;
    --mesen-registry)
      MESEN_REGISTRY_SCRIPT="$2"
      shift 2
      ;;
    --sanity-script)
      SANITY_SCRIPT="$2"
      shift 2
      ;;
    --sanity-every)
      if [[ -z "${2:-}" ]]; then
        echo "Error: --sanity-every requires a number" >&2
        exit 1
      fi
      if ! [[ "$2" =~ ^[0-9]+$ ]]; then
        echo "Error: --sanity-every must be an integer" >&2
        exit 1
      fi
      SANITY_EVERY="$2"
      shift 2
      ;;
    --sanity-strict)
      SANITY_STRICT=1
      shift
      ;;
    --no-sanity)
      SANITY_ENABLED=0
      shift
      ;;
    --preflight-script)
      PREFLIGHT_SCRIPT="$2"
      shift 2
      ;;
    --preflight-every)
      if [[ -z "${2:-}" ]]; then
        echo "Error: --preflight-every requires a number" >&2
        exit 1
      fi
      if ! [[ "$2" =~ ^[0-9]+$ ]]; then
        echo "Error: --preflight-every must be an integer" >&2
        exit 1
      fi
      PREFLIGHT_EVERY="$2"
      shift 2
      ;;
    --preflight-strict)
      PREFLIGHT_STRICT=1
      shift
      ;;
    --preflight-rebuild-dirty)
      PREFLIGHT_REBUILD_DIRTY=1
      shift
      ;;
    --no-preflight)
      PREFLIGHT_ENABLED=0
      shift
      ;;
    --no-guardrails)
      INJECT_GUARDRAILS=0
      shift
      ;;
    --no-color)
      USE_COLOR=0
      shift
      ;;
    --color)
      USE_COLOR=1
      shift
      ;;
    --no-prefix-output)
      PREFIX_OUTPUT=0
      shift
      ;;
    --prefix-tag)
      PREFIX_TAG="$2"
      shift 2
      ;;
    --)
      shift
      CODEX_ARGS+=("$@")
      break
      ;;
    *)
      PROMPT_PARTS+=("$1")
      shift
      ;;
  esac
done

ROOT_DIR=$(cd "$ROOT_DIR" && pwd)
if [[ -d "$OOS_ROOT" ]]; then
  OOS_ROOT=$(cd "$OOS_ROOT" && pwd)
fi
if [[ -z "${MESEN_REGISTRY_SCRIPT}" ]]; then
  if [[ -f "${OOS_ROOT}/scripts/mesen2_registry.py" ]]; then
    MESEN_REGISTRY_SCRIPT="${OOS_ROOT}/scripts/mesen2_registry.py"
  elif [[ -f "${ROOT_DIR}/scripts/mesen2_registry.py" ]]; then
    MESEN_REGISTRY_SCRIPT="${ROOT_DIR}/scripts/mesen2_registry.py"
  fi
fi
if [[ -z "$SANITY_SCRIPT" ]]; then
  if [[ -f "${OOS_ROOT}/scripts/mesen2_sanity_check.sh" ]]; then
    SANITY_SCRIPT="${OOS_ROOT}/scripts/mesen2_sanity_check.sh"
  elif [[ -f "${ROOT_DIR}/scripts/mesen2_sanity_check.sh" ]]; then
    SANITY_SCRIPT="${ROOT_DIR}/scripts/mesen2_sanity_check.sh"
  fi
fi
if [[ -z "$PREFLIGHT_SCRIPT" ]]; then
  if [[ -f "${OOS_ROOT}/scripts/mesen2_preflight.sh" ]]; then
    PREFLIGHT_SCRIPT="${OOS_ROOT}/scripts/mesen2_preflight.sh"
  elif [[ -f "${ROOT_DIR}/scripts/mesen2_preflight.sh" ]]; then
    PREFLIGHT_SCRIPT="${ROOT_DIR}/scripts/mesen2_preflight.sh"
  fi
fi

if [[ -n "$PROMPT_FILE" ]]; then
  if [[ ! -f "$PROMPT_FILE" ]]; then
    echo "Error: prompt file not found: $PROMPT_FILE" >&2
    exit 1
  fi
  PROMPT_TEXT=$(cat "$PROMPT_FILE")
else
  PROMPT_TEXT="${PROMPT_PARTS[*]-}"
fi

if [[ -z "$PROMPT_TEXT" ]]; then
  echo "Error: No prompt provided" >&2
  usage
  exit 1
fi

if [[ "${NO_COLOR:-}" != "" ]]; then
  USE_COLOR=0
fi
if [[ ! -t 1 ]]; then
  USE_COLOR=0
fi
if [[ "${TERM:-dumb}" == "dumb" ]]; then
  USE_COLOR=0
fi

if [[ $USE_COLOR -eq 1 ]]; then
  COLOR_BOLD=$'\033[1m'
  COLOR_DIM=$'\033[2m'
  COLOR_RED=$'\033[31m'
  COLOR_GREEN=$'\033[32m'
  COLOR_YELLOW=$'\033[33m'
  COLOR_BLUE=$'\033[34m'
  COLOR_RESET=$'\033[0m'
else
  COLOR_BOLD=""
  COLOR_DIM=""
  COLOR_RED=""
  COLOR_GREEN=""
  COLOR_YELLOW=""
  COLOR_BLUE=""
  COLOR_RESET=""
fi

log_hr() {
  printf '%s\n' "${COLOR_DIM}------------------------------------------------------------${COLOR_RESET}"
}

log_section() {
  log_hr
  printf '%s%s%s\n' "$COLOR_BOLD" "$1" "$COLOR_RESET"
  log_hr
}

log_info() {
  printf '%s%s%s\n' "$COLOR_BLUE" "$1" "$COLOR_RESET"
}

log_warn() {
  printf '%s%s%s\n' "$COLOR_YELLOW" "$1" "$COLOR_RESET"
}

log_error() {
  printf '%s%s%s\n' "$COLOR_RED" "$1" "$COLOR_RESET"
}

stat_mtime() {
  local path="$1"
  if [[ "$(uname)" == "Darwin" ]]; then
    stat -f %m "$path" 2>/dev/null || echo 0
  else
    stat -c %Y "$path" 2>/dev/null || echo 0
  fi
}

current_pgid() {
  ps -o pgid= -p $$ 2>/dev/null | tr -d '[:space:]'
}

yaml_quote() {
  local value="$1"
  value=${value//\\/\\\\}
  value=${value//\"/\\\"}
  printf "\"%s\"" "$value"
}

yaml_bool() {
  if [[ "$1" -eq 1 ]]; then
    echo "true"
  else
    echo "false"
  fi
}

DEFAULT_GUARDRAILS=$(cat <<'EOF'

[Loop Guardrails]
- If the repo is dirty, do not block on questions. Run tests/docs only and log a brief summary to the coordination board.
- One outcome per iteration: end with a single line `Outcome: ...` (or `Outcome: no-op (reason)`).
- Always perform a micro-task each iteration (tiny unit filter, smoke script, or doc update). If unsafe, note why.
- If Mesen2-OOS is used, launch the patched ROM `~/src/hobby/oracle-of-secrets/Roms/oos168x.sfc` (never `oos168_test2.sfc` or the clean base).
- When using Mesen2-OOS, prioritize fork features (socket API + registry) and multi-instance titled windows via `~/src/hobby/oracle-of-secrets/scripts/mesen_launch.sh --multi --instance <name> --title "<agent>" --source "<source>"` or `MESEN2_AGENT_TITLE` + `MESEN2_AGENT_SOURCE`.
- AFS CLI writes to ~/.context/projects/yaze are blocked by the sandbox; use repo .context scratchpad updates instead.
EOF
)

build_prompt_payload() {
  local prompt="$1"
  local guardrails="$2"
  if [[ -z "$guardrails" ]]; then
    printf "%s" "$prompt"
    return
  fi
  if [[ -z "$prompt" || "$prompt" == *$'\n' ]]; then
    printf "%s%s" "$prompt" "$guardrails"
  else
    printf "%s\n%s" "$prompt" "$guardrails"
  fi
}

build_guardrails() {
  local repo_dirty="$1"
  local repo_status="$2"
  local guardrails=""

  if [[ $INJECT_GUARDRAILS -eq 1 ]]; then
    guardrails="$DEFAULT_GUARDRAILS"
    if [[ -n "$GUARDRAILS_FILE" ]]; then
      if [[ ! -f "$GUARDRAILS_FILE" ]]; then
        echo "Error: guardrails file not found: $GUARDRAILS_FILE" >&2
        exit 1
      fi
      guardrails+=$'\n\n'
      guardrails+="$(cat "$GUARDRAILS_FILE")"
    fi
    if [[ -n "${PREFERENCES_FILE}" && -f "${PREFERENCES_FILE}" ]]; then
      guardrails+=$'\n\n'
      guardrails+="$(cat "$PREFERENCES_FILE")"
    fi
    if [[ "$repo_dirty" -eq 1 ]]; then
      guardrails+=$'\n\n'
      guardrails+="[Repo Status]\n${repo_status}\n"
    fi
  fi

  echo "$guardrails"
}

write_state() {
  local now
  now="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  local active_yaml
  active_yaml="$(yaml_bool "$STATE_ACTIVE")"
  local watchdog_yaml
  watchdog_yaml="$(yaml_bool "$WATCHDOG_TRIGGERED")"

  local ended_at_value
  if [[ -n "$STATE_ENDED_AT" ]]; then
    ended_at_value="$(yaml_quote "$STATE_ENDED_AT")"
  else
    ended_at_value="null"
  fi

  cat > "$STATE_FILE" <<EOF
---
active: $active_yaml
loop_kind: "codex"
iteration: $ITERATION
max_iterations: $MAX_ITERATIONS
completion_promise: $COMPLETION_PROMISE_YAML
root_dir: $(yaml_quote "$ROOT_DIR")
oos_root: $(yaml_quote "$OOS_ROOT")
prompt_file: $(yaml_quote "$PROMPT_FILE")
guardrails_file: $(yaml_quote "$GUARDRAILS_FILE")
preferences_file: $(yaml_quote "$PREFERENCES_FILE")
mesen_instance: $(yaml_quote "$MESEN_REGISTRY_INSTANCE")
mesen_owner: $(yaml_quote "$MESEN_REGISTRY_OWNER")
mesen_rom: $(yaml_quote "$MESEN_REGISTRY_ROM")
mesen_socket: $(yaml_quote "$MESEN_REGISTRY_SOCKET")
mesen_registry: $(yaml_quote "$MESEN_REGISTRY_SCRIPT")
mesen_socket_resolved: $(yaml_quote "$RESOLVED_SOCKET")
mesen_socket_source: $(yaml_quote "$RESOLVED_SOCKET_SOURCE")
mesen_socket_resolved_at: $(yaml_quote "$RESOLVED_SOCKET_AT")
socket_recovery_status: $(yaml_quote "$SOCKET_RECOVERY_STATUS")
socket_recovery_socket: $(yaml_quote "$SOCKET_RECOVERY_SOCKET")
socket_recovery_at: $(yaml_quote "$SOCKET_RECOVERY_AT")
codex_args: $(yaml_quote "${CODEX_ARGS[*]}")
sanity_enabled: $(yaml_bool "$SANITY_ENABLED")
sanity_every: $SANITY_EVERY
sanity_strict: $(yaml_bool "$SANITY_STRICT")
sanity_script: $(yaml_quote "$SANITY_SCRIPT")
preflight_enabled: $(yaml_bool "$PREFLIGHT_ENABLED")
preflight_every: $PREFLIGHT_EVERY
preflight_strict: $(yaml_bool "$PREFLIGHT_STRICT")
preflight_rebuild_dirty: $(yaml_bool "$PREFLIGHT_REBUILD_DIRTY")
preflight_script: $(yaml_quote "$PREFLIGHT_SCRIPT")
last_sanity_status: $(yaml_quote "$LAST_SANITY_STATUS")
last_sanity_code: $LAST_SANITY_CODE
last_preflight_status: $(yaml_quote "$LAST_PREFLIGHT_STATUS")
last_preflight_code: $LAST_PREFLIGHT_CODE
last_exit_status: $LAST_EXIT_STATUS
last_outcome_status: $(yaml_quote "$LAST_OUTCOME_STATUS")
last_micro_task_status: $(yaml_quote "$LAST_MICRO_TASK_STATUS")
last_message_empty: $(yaml_quote "$LAST_MESSAGE_EMPTY")
watchdog_triggered: $watchdog_yaml
runner_pid: $$
runner_pgid: $(current_pgid)
output_file: $(yaml_quote "$OUTPUT_FILE")
log_file: $(yaml_quote "$LOG_FILE")
last_message_file: $(yaml_quote "$LAST_MESSAGE_FILE")
heartbeat_file: $(yaml_quote "$HEARTBEAT_FILE")
watchdog_file: $(yaml_quote "$WATCHDOG_FILE")
activity_file: $(yaml_quote "$ACTIVITY_FILE")
repo_status_file: $(yaml_quote "$REPO_STATUS_FILE")
index_file: $(yaml_quote "$INDEX_FILE")
combined_log_file: $(yaml_quote "$COMBINED_LOG_FILE")
started_at: $(yaml_quote "${ITERATION_STARTED_AT:-$now}")
updated_at: $(yaml_quote "$now")
ended_at: $ended_at_value
---

${STATE_BODY}
EOF

  STATE_WRITTEN=1
}

on_exit() {
  trap - EXIT
  set +e
  if [[ $STATE_WRITTEN -eq 1 ]]; then
    STATE_ACTIVE=0
    STATE_ENDED_AT="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    write_state
  fi
}

trap on_exit EXIT

mesen_registry_enabled() {
  [[ -n "${MESEN_REGISTRY_INSTANCE}" && -n "${MESEN_REGISTRY_OWNER}" && -f "${MESEN_REGISTRY_SCRIPT}" ]]
}

mesen_registry_claim() {
  if ! mesen_registry_enabled; then
    return 0
  fi
  local args=(claim --instance "${MESEN_REGISTRY_INSTANCE}" --owner "${MESEN_REGISTRY_OWNER}")
  local socket_arg="${MESEN_REGISTRY_SOCKET}"
  local pid_arg="${MESEN_REGISTRY_PID}"
  if [[ -n "${socket_arg}" && ! -S "${socket_arg}" ]]; then
    socket_arg=""
  fi
  if [[ -n "${pid_arg}" ]]; then
    local pid_socket="/tmp/mesen2-${pid_arg}.sock"
    if [[ ! -S "${pid_socket}" ]]; then
      pid_arg=""
    fi
  fi
  if [[ -n "${socket_arg}" ]]; then
    args+=(--socket "${socket_arg}")
  elif [[ -n "${pid_arg}" ]]; then
    args+=(--pid "${pid_arg}")
  elif [[ -n "${MESEN_REGISTRY_ROM}" ]]; then
    args+=(--rom "${MESEN_REGISTRY_ROM}")
  fi
  python3 "${MESEN_REGISTRY_SCRIPT}" "${args[@]}" >/dev/null 2>&1 || true
}

mesen_registry_heartbeat() {
  if [[ -z "${MESEN_REGISTRY_INSTANCE}" || ! -f "${MESEN_REGISTRY_SCRIPT}" ]]; then
    return 0
  fi
  python3 "${MESEN_REGISTRY_SCRIPT}" heartbeat --instance "${MESEN_REGISTRY_INSTANCE}" >/dev/null 2>&1 || true
}

update_mesen_resolved_socket() {
  RESOLVED_SOCKET=""
  RESOLVED_SOCKET_SOURCE=""
  RESOLVED_SOCKET_AT=""

  if [[ -n "${MESEN_REGISTRY_SOCKET}" && -S "${MESEN_REGISTRY_SOCKET}" ]]; then
    RESOLVED_SOCKET="${MESEN_REGISTRY_SOCKET}"
    RESOLVED_SOCKET_SOURCE="arg"
  elif [[ -n "${MESEN2_SOCKET_PATH:-}" && -S "${MESEN2_SOCKET_PATH}" ]]; then
    RESOLVED_SOCKET="${MESEN2_SOCKET_PATH}"
    RESOLVED_SOCKET_SOURCE="env"
  elif [[ -n "${MESEN_REGISTRY_INSTANCE}" && -f "${MESEN_REGISTRY_SCRIPT}" ]]; then
    local resolved
    resolved="$(python3 "${MESEN_REGISTRY_SCRIPT}" resolve --instance "${MESEN_REGISTRY_INSTANCE}" 2>/dev/null || true)"
    if [[ -n "${resolved}" && -S "${resolved}" ]]; then
      RESOLVED_SOCKET="${resolved}"
      RESOLVED_SOCKET_SOURCE="registry"
    fi
  fi

  if [[ -n "${RESOLVED_SOCKET}" ]]; then
    RESOLVED_SOCKET_AT="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  fi
}

apply_mesen_env() {
  if [[ -n "${MESEN_REGISTRY_INSTANCE}" ]]; then
    export MESEN2_INSTANCE="${MESEN_REGISTRY_INSTANCE}"
  else
    unset MESEN2_INSTANCE
  fi

  if [[ -n "${RESOLVED_SOCKET}" && -S "${RESOLVED_SOCKET}" ]]; then
    export MESEN2_SOCKET_PATH="${RESOLVED_SOCKET}"
  else
    unset MESEN2_SOCKET_PATH
  fi
}

attempt_socket_recovery() {
  SOCKET_RECOVERY_STATUS="not_needed"
  SOCKET_RECOVERY_SOCKET=""
  SOCKET_RECOVERY_AT=""
  SANITY_RETRY_NEEDED=0

  if [[ "$LAST_SANITY_STATUS" != "error" ]]; then
    return 0
  fi

  if [[ -z "${MESEN_REGISTRY_SCRIPT}" || ! -f "${MESEN_REGISTRY_SCRIPT}" ]]; then
    SOCKET_RECOVERY_STATUS="no_registry"
    return 0
  fi
  if [[ -z "${MESEN_REGISTRY_INSTANCE}" || -z "${MESEN_REGISTRY_OWNER}" ]]; then
    SOCKET_RECOVERY_STATUS="missing_identity"
    return 0
  fi

  local rom_base=""
  if [[ -n "${MESEN_REGISTRY_ROM}" ]]; then
    rom_base="$(basename "${MESEN_REGISTRY_ROM}")"
  fi

  local socket
  socket="$(python3 - "${MESEN_REGISTRY_SCRIPT}" "${rom_base}" <<'PY' 2>/dev/null || true
import json
import subprocess
import sys

registry = sys.argv[1]
rom_base = sys.argv[2] if len(sys.argv) > 2 else ""

try:
    out = subprocess.check_output([sys.executable, registry, "scan", "--json"], text=True)
    data = json.loads(out)
except Exception:
    sys.exit(0)

for entry in data:
    if not entry.get("alive"):
        continue
    if rom_base and entry.get("rom_filename") != rom_base:
        continue
    sock = entry.get("socket")
    if sock:
        print(sock)
        sys.exit(0)
sys.exit(0)
PY
)"

  if [[ -z "${socket}" ]]; then
    SOCKET_RECOVERY_STATUS="no_socket"
    return 0
  fi

  python3 "${MESEN_REGISTRY_SCRIPT}" claim --instance "${MESEN_REGISTRY_INSTANCE}" \
    --owner "${MESEN_REGISTRY_OWNER}" --socket "${socket}" \
    ${MESEN_REGISTRY_ROM:+--rom "${MESEN_REGISTRY_ROM}"} >/dev/null 2>&1 || true

  MESEN_REGISTRY_SOCKET="${socket}"
  SOCKET_RECOVERY_STATUS="claimed"
  SOCKET_RECOVERY_SOCKET="${socket}"
  SOCKET_RECOVERY_AT="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  SANITY_RETRY_NEEDED=1
}

run_sanity_check() {
  LAST_SANITY_STATUS="disabled"
  LAST_SANITY_CODE="null"
  if [[ $SANITY_ENABLED -ne 1 ]]; then
    return 0
  fi
  if [[ $SANITY_EVERY -gt 0 ]] && (( ITERATION % SANITY_EVERY != 0 )); then
    LAST_SANITY_STATUS="skipped"
    return 0
  fi
  if [[ -z "$SANITY_SCRIPT" || ! -f "$SANITY_SCRIPT" ]]; then
    LAST_SANITY_STATUS="missing"
    return 0
  fi
  local cmd=(bash "$SANITY_SCRIPT" --root "$OOS_ROOT")
  if [[ -n "$MESEN_REGISTRY_INSTANCE" ]]; then
    cmd+=(--instance "$MESEN_REGISTRY_INSTANCE")
  fi
  if [[ -n "$MESEN_REGISTRY_OWNER" ]]; then
    cmd+=(--owner "$MESEN_REGISTRY_OWNER")
  fi
  if [[ -n "$MESEN_REGISTRY_ROM" ]]; then
    cmd+=(--rom "$MESEN_REGISTRY_ROM")
  fi
  if [[ -n "$MESEN_REGISTRY_SOCKET" ]]; then
    cmd+=(--socket "$MESEN_REGISTRY_SOCKET")
  fi
  if [[ -n "$MESEN_REGISTRY_SCRIPT" ]]; then
    cmd+=(--registry "$MESEN_REGISTRY_SCRIPT")
  fi
  if [[ $SANITY_STRICT -eq 1 ]]; then
    cmd+=(--strict)
  fi
  echo "=== Mesen2 sanity check ===" >> "$LOG_FILE"
  "${cmd[@]}" >> "$LOG_FILE" 2>&1
  local status=$?
  LAST_SANITY_CODE=$status
  if [[ $status -ne 0 ]]; then
    LAST_SANITY_STATUS="error"
    log_warn "Mesen2 sanity check failed (status $status)."
    echo "sanity_check_status: $status" >> "$LOG_FILE"
  else
    LAST_SANITY_STATUS="ok"
  fi
}

run_preflight() {
  LAST_PREFLIGHT_STATUS="disabled"
  LAST_PREFLIGHT_CODE="null"
  if [[ $PREFLIGHT_ENABLED -ne 1 ]]; then
    return 0
  fi
  if [[ $PREFLIGHT_EVERY -gt 0 ]] && (( ITERATION % PREFLIGHT_EVERY != 0 )); then
    LAST_PREFLIGHT_STATUS="skipped"
    return 0
  fi
  if [[ -z "$PREFLIGHT_SCRIPT" || ! -f "$PREFLIGHT_SCRIPT" ]]; then
    LAST_PREFLIGHT_STATUS="missing"
    return 0
  fi
  local cmd=(bash "$PREFLIGHT_SCRIPT" --root "$OOS_ROOT")
  if [[ -n "$MESEN_REGISTRY_ROM" ]]; then
    cmd+=(--rom "$MESEN_REGISTRY_ROM")
  fi
  if [[ $PREFLIGHT_STRICT -eq 1 ]]; then
    cmd+=(--strict)
  fi
  if [[ $PREFLIGHT_REBUILD_DIRTY -eq 1 ]]; then
    cmd+=(--rebuild-dirty)
  fi
  echo "=== Mesen2 preflight ===" >> "$LOG_FILE"
  "${cmd[@]}" >> "$LOG_FILE" 2>&1
  local status=$?
  LAST_PREFLIGHT_CODE=$status
  if [[ $status -ne 0 ]]; then
    LAST_PREFLIGHT_STATUS="error"
    log_warn "Mesen2 preflight failed (status $status)."
    echo "preflight_status: $status" >> "$LOG_FILE"
  else
    LAST_PREFLIGHT_STATUS="ok"
  fi
}

if [[ ${#CODEX_ARGS[@]} -eq 0 ]]; then
  CODEX_ARGS=(--full-auto)
fi

if [[ "$STATE_FILE" != /* ]]; then
  STATE_FILE="$ROOT_DIR/$STATE_FILE"
fi
if [[ "$OUTPUT_DIR" != /* ]]; then
  OUTPUT_DIR="$ROOT_DIR/$OUTPUT_DIR"
fi

mkdir -p "$(dirname "$STATE_FILE")"
mkdir -p "$OUTPUT_DIR"
INDEX_FILE="$OUTPUT_DIR/index.md"
COMBINED_LOG_FILE="$OUTPUT_DIR/combined.log"
if [[ -z "$REPORT_SCRIPT" ]]; then
  REPORT_SCRIPT="$ROOT_DIR/scripts/agents/ralph-loop-report.sh"
fi

if [[ -n "$COMPLETION_PROMISE" ]]; then
  COMPLETION_PROMISE_YAML="\"$COMPLETION_PROMISE\""
else
  COMPLETION_PROMISE_YAML="null"
fi

ITERATION=1

while true; do
  if [[ $MAX_ITERATIONS -gt 0 ]] && [[ $ITERATION -gt $MAX_ITERATIONS ]]; then
    if [[ $AUTO_EXTEND -eq 1 ]] && [[ -n "$COMPLETION_PROMISE" ]]; then
      MAX_ITERATIONS=$((MAX_ITERATIONS + EXTEND_BY))
      echo "Ralph loop: auto-extended max iterations to $MAX_ITERATIONS."
    else
      echo "Ralph loop: max iterations ($MAX_ITERATIONS) reached."
      break
    fi
  fi

  OUTPUT_FILE="$OUTPUT_DIR/iteration-${ITERATION}.last.txt"
  LOG_FILE="$OUTPUT_DIR/iteration-${ITERATION}.log"
  LAST_MESSAGE_FILE="$OUTPUT_FILE"
  HEARTBEAT_FILE="$OUTPUT_DIR/iteration-${ITERATION}.heartbeat"
  WATCHDOG_FILE="$OUTPUT_DIR/iteration-${ITERATION}.watchdog"
  ACTIVITY_FILE="$OUTPUT_DIR/iteration-${ITERATION}.activity"
  REPO_STATUS_FILE="$OUTPUT_DIR/iteration-${ITERATION}.repo.txt"
  : > "$LAST_MESSAGE_FILE"
  : > "$HEARTBEAT_FILE"
  : > "$WATCHDOG_FILE"
  : > "$ACTIVITY_FILE"
  : > "$REPO_STATUS_FILE"
  : > "$LOG_FILE"

  REPO_DIRTY=0
  REPO_STATUS=""
  if git -C "$ROOT_DIR" rev-parse --git-dir >/dev/null 2>&1; then
    REPO_STATUS=$(git -C "$ROOT_DIR" status -sb 2>/dev/null | head -n 50 || true)
    if [[ -n "$REPO_STATUS" ]]; then
      printf "%s\n" "$REPO_STATUS" > "$REPO_STATUS_FILE"
      if [[ $(echo "$REPO_STATUS" | wc -l | tr -d '[:space:]') -gt 1 ]]; then
        REPO_DIRTY=1
      fi
    fi
  fi

  GUARDRAILS_TEXT="$(build_guardrails "$REPO_DIRTY" "$REPO_STATUS")"
  mesen_registry_claim
  update_mesen_resolved_socket
  apply_mesen_env

  WATCHDOG_TRIGGERED=0
  LAST_PREFLIGHT_STATUS="pending"
  LAST_PREFLIGHT_CODE="null"
  LAST_SANITY_STATUS="pending"
  LAST_SANITY_CODE="null"
  LAST_EXIT_STATUS="null"
  LAST_OUTCOME_STATUS="unknown"
  LAST_MICRO_TASK_STATUS="unknown"
  LAST_MESSAGE_EMPTY="unknown"
  SOCKET_RECOVERY_STATUS="not_run"
  SOCKET_RECOVERY_SOCKET=""
  SOCKET_RECOVERY_AT=""
  STATE_ACTIVE=1
  STATE_ENDED_AT=""
  ITERATION_STARTED_AT="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  STATE_BODY="$(build_prompt_payload "$PROMPT_TEXT" "$GUARDRAILS_TEXT")"
  write_state

  log_section "Ralph (Codex) iteration $ITERATION"
  log_info "Started: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  log_info "Root: $ROOT_DIR"
  log_info "Log: $LOG_FILE"
  log_info "Last message: $LAST_MESSAGE_FILE"
  if [[ $REPO_DIRTY -eq 1 ]]; then
    log_warn "Repo status captured: $REPO_STATUS_FILE"
  else
    log_info "Repo status captured: $REPO_STATUS_FILE"
  fi
  set +e
  ATTEMPT=0
  while true; do
    ATTEMPT=$((ATTEMPT + 1))
    rm -f "$WATCHDOG_FILE"
    : > "$LOG_FILE"
    run_preflight
    run_sanity_check
    attempt_socket_recovery
    if [[ $SANITY_RETRY_NEEDED -eq 1 ]]; then
      run_sanity_check
    fi
    update_mesen_resolved_socket
    apply_mesen_env
    write_state

    if [[ $HEARTBEAT_SECONDS -gt 0 ]]; then
      (
        while true; do
          sleep "$HEARTBEAT_SECONDS"
          ts="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
          echo "$ts" > "$HEARTBEAT_FILE"
          mesen_registry_heartbeat
        done
      ) &
      HEARTBEAT_PID=$!
    fi

    PIPE_PATH="$OUTPUT_DIR/iteration-${ITERATION}.pipe"
    rm -f "$PIPE_PATH"
    mkfifo "$PIPE_PATH"

    if [[ $STREAM_OUTPUT -eq 1 ]]; then
      LOG_PREFIX=""
      if [[ $PREFIX_OUTPUT -eq 1 ]]; then
        LOG_PREFIX="[${PREFIX_TAG}:${ITERATION}] "
      fi
      awk -v stampfile="$ACTIVITY_FILE" -v prefix="$LOG_PREFIX" '{ print prefix $0; print strftime("%Y-%m-%dT%H:%M:%SZ") > stampfile; fflush(stampfile); fflush(); }' < "$PIPE_PATH" | tee "$LOG_FILE" &
    else
      LOG_PREFIX=""
      if [[ $PREFIX_OUTPUT -eq 1 ]]; then
        LOG_PREFIX="[${PREFIX_TAG}:${ITERATION}] "
      fi
      awk -v log_file="$LOG_FILE" -v stampfile="$ACTIVITY_FILE" -v prefix="$LOG_PREFIX" '{ print prefix $0 >> log_file; print strftime("%Y-%m-%dT%H:%M:%SZ") > stampfile; fflush(log_file); fflush(stampfile) }' < "$PIPE_PATH" &
    fi
    READER_PID=$!

    printf "%s" "$(build_prompt_payload "$PROMPT_TEXT" "$GUARDRAILS_TEXT")" | codex exec -C "$ROOT_DIR" --output-last-message "$LAST_MESSAGE_FILE" "${CODEX_ARGS[@]}" - > "$PIPE_PATH" 2>&1 &
    CODEX_PID=$!

    if [[ $WATCHDOG_SECONDS -gt 0 ]]; then
      (
        last_mtime=$(stat_mtime "$LOG_FILE")
        while kill -0 "$CODEX_PID" 2>/dev/null; do
          sleep "$WATCHDOG_SECONDS"
          now_mtime=$(stat_mtime "$LOG_FILE")
          if [[ "$now_mtime" -le "$last_mtime" ]]; then
            ts="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
            echo "watchdog: no output for ${WATCHDOG_SECONDS}s at ${ts}, restarting iteration." > "$WATCHDOG_FILE"
            echo "watchdog: no output for ${WATCHDOG_SECONDS}s at ${ts}, killing codex exec." >> "$LOG_FILE"
            kill "$CODEX_PID" 2>/dev/null || true
            break
          fi
          last_mtime="$now_mtime"
        done
      ) &
      WATCHDOG_PID=$!
    fi

    wait "$CODEX_PID"
    CODEX_STATUS=$?

    rm -f "$PIPE_PATH"

    if [[ -n "${HEARTBEAT_PID:-}" ]]; then
      kill "$HEARTBEAT_PID" 2>/dev/null || true
      wait "$HEARTBEAT_PID" 2>/dev/null || true
      HEARTBEAT_PID=""
    fi

    if [[ -n "${WATCHDOG_PID:-}" ]]; then
      kill "$WATCHDOG_PID" 2>/dev/null || true
      wait "$WATCHDOG_PID" 2>/dev/null || true
      WATCHDOG_PID=""
    fi

    if [[ -n "${READER_PID:-}" ]]; then
      wait "$READER_PID" 2>/dev/null || true
      READER_PID=""
    fi

    if [[ -f "$WATCHDOG_FILE" ]]; then
      WATCHDOG_TRIGGERED=1
      echo "Warning: watchdog triggered for iteration $ITERATION (attempt $ATTEMPT)." >> "$LOG_FILE"
      log_warn "Watchdog triggered (attempt $ATTEMPT/$WATCHDOG_RETRIES)."
      if [[ $ATTEMPT -le $WATCHDOG_RETRIES ]]; then
        log_warn "Ralph loop: watchdog restart for iteration $ITERATION (attempt $((ATTEMPT + 1))/${WATCHDOG_RETRIES})."
        continue
      fi
    fi
    break
  done
  set -e

  if [[ $CODEX_STATUS -ne 0 ]]; then
    log_warn "codex exec exited with $CODEX_STATUS"
    if [[ $STOP_ON_ERROR -eq 1 ]]; then
      exit $CODEX_STATUS
    fi
  fi

  LAST_MESSAGE_EMPTY="false"
  OUTCOME_STATUS="missing"
  MICRO_TASK_STATUS="missing"
  if [[ -f "$LAST_MESSAGE_FILE" ]]; then
    if [[ -n "$(tr -d '[:space:]' < "$LAST_MESSAGE_FILE" 2>/dev/null || true)" ]]; then
      if grep -q '^Outcome:' "$LAST_MESSAGE_FILE" 2>/dev/null; then
        OUTCOME_STATUS="ok"
      fi
      if grep -qi '^Micro-task:' "$LAST_MESSAGE_FILE" 2>/dev/null; then
        MICRO_TASK_STATUS="ok"
      fi
    else
      LAST_MESSAGE_EMPTY="true"
    fi
  else
    LAST_MESSAGE_EMPTY="true"
  fi
  LAST_EXIT_STATUS=$CODEX_STATUS
  LAST_OUTCOME_STATUS="$OUTCOME_STATUS"
  LAST_MICRO_TASK_STATUS="$MICRO_TASK_STATUS"
  write_state

  {
    echo "===== Iteration $ITERATION ====="
    echo "Started: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "ExitStatus: $CODEX_STATUS"
    echo "LogFile: $LOG_FILE"
    echo "LastMessageFile: $LAST_MESSAGE_FILE"
    echo "RepoStatusFile: $REPO_STATUS_FILE"
    echo "RepoDirty: $REPO_DIRTY"
    echo "OutcomeStatus: $OUTCOME_STATUS"
    echo "MicroTaskStatus: $MICRO_TASK_STATUS"
    echo "LastMessageEmpty: $LAST_MESSAGE_EMPTY"
    if [[ -f "$WATCHDOG_FILE" ]] && [[ -s "$WATCHDOG_FILE" ]]; then
      echo "Watchdog: $(cat "$WATCHDOG_FILE")"
    fi
    echo "----- LastMessage -----"
    if [[ -f "$LAST_MESSAGE_FILE" ]]; then
      cat "$LAST_MESSAGE_FILE"
    else
      echo "(missing last message file)"
    fi
    echo "----- EndLastMessage -----"
    echo
  } >> "$INDEX_FILE"

  {
    echo "===== Iteration $ITERATION ====="
    echo "Started: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    if [[ -f "$LOG_FILE" ]]; then
      cat "$LOG_FILE"
    else
      echo "(missing log file)"
    fi
    if [[ -f "$WATCHDOG_FILE" ]] && [[ -s "$WATCHDOG_FILE" ]]; then
      echo "watchdog-note: $(cat "$WATCHDOG_FILE")"
    fi
    echo
  } >> "$COMBINED_LOG_FILE"

  log_hr
  if [[ $CODEX_STATUS -eq 0 ]]; then
    log_info "ExitStatus: ok"
  else
    log_warn "ExitStatus: $CODEX_STATUS"
  fi
  log_info "Outcome: $OUTCOME_STATUS | Micro-task: $MICRO_TASK_STATUS | Repo dirty: $REPO_DIRTY | Last message empty: $LAST_MESSAGE_EMPTY"
  if [[ -f "$WATCHDOG_FILE" ]] && [[ -s "$WATCHDOG_FILE" ]]; then
    log_warn "Watchdog: $(cat "$WATCHDOG_FILE")"
  fi

  if [[ $PRINT_LAST_LINES -gt 0 ]] && [[ -f "$LAST_MESSAGE_FILE" ]]; then
    log_hr
    log_info "Last message (first ${PRINT_LAST_LINES} lines)"
    sed -n "1,${PRINT_LAST_LINES}p" "$LAST_MESSAGE_FILE"
  fi

  if [[ $TAIL_LINES -gt 0 ]] && [[ -f "$LOG_FILE" ]]; then
    log_hr
    log_info "Log tail (last ${TAIL_LINES} lines)"
    tail -n "$TAIL_LINES" "$LOG_FILE"
  fi

  if [[ $REPORT_EVERY -gt 0 ]] && (( ITERATION % REPORT_EVERY == 0 )); then
    if [[ -x "$REPORT_SCRIPT" ]]; then
      "$REPORT_SCRIPT" --root "$ROOT_DIR" --limit "$REPORT_LIMIT" >/dev/null 2>&1 || true
      echo "Ralph report updated (last ${REPORT_LIMIT} iterations)."
    fi
  fi

  if [[ -n "$COMPLETION_PROMISE" ]]; then
    LAST_OUTPUT=$(cat "$LAST_MESSAGE_FILE" 2>/dev/null || true)
    PROMISE_TEXT=$(echo "$LAST_OUTPUT" | perl -0777 -pe 's/.*?<promise>(.*?)<\/promise>.*/$1/s; s/^\s+|\s+$//g; s/\s+/ /g' 2>/dev/null || echo "")
    if [[ -n "$PROMISE_TEXT" ]] && [[ "$PROMISE_TEXT" = "$COMPLETION_PROMISE" ]]; then
      echo "Ralph loop complete: <promise>$COMPLETION_PROMISE</promise>"
      break
    fi
  fi

  ITERATION=$((ITERATION + 1))
  sleep 1

  # Refresh prompt text from file if provided
  if [[ -n "$PROMPT_FILE" ]]; then
    PROMPT_TEXT=$(cat "$PROMPT_FILE")
  fi

done

exit 0
