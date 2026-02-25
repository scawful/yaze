#!/usr/bin/env bash
# ai-provider-matrix-smoke.sh
#
# Runs a lightweight z3ed agent prompt across multiple providers so we can
# validate rollout stability for Claude/OpenAI/Gemini/Ollama/custom endpoints.

set -euo pipefail

BUILD_DIR="build_ai"
ROM_PATH=""
PROVIDERS="mock,ollama,gemini,claude,chatgpt,lmstudio"
PROMPT="List one command to inspect dungeon room 0x77 sprites."
MODEL=""
OUTPUT_PATH=""
TIMEOUT_SECONDS=25

usage() {
  cat <<USAGE
Usage: scripts/dev/ai-provider-matrix-smoke.sh [options]

Options:
  --build-dir <dir>     Build directory containing z3ed (default: build_ai)
  --rom <path>          ROM path to use (default: --mock-rom)
  --providers <csv>     Provider list (default: mock,ollama,gemini,claude,chatgpt,lmstudio)
  --prompt <text>       Prompt to run (default: dungeon sprite query)
  --model <name>        Optional --ai_model override for all providers
  --timeout <seconds>   Per-provider timeout (default: 25)
  --output <path>       Optional JSON output path
  -h, --help            Show this help

Readiness checks:
  - gemini: requires GEMINI_API_KEY
  - claude/anthropic: requires ANTHROPIC_API_KEY
  - chatgpt/openai/lmstudio: requires OPENAI_API_KEY or OPENAI_BASE_URL
  - ollama: requires OLLAMA_HOST or local ollama binary
  - mock: always available
USAGE
}

trim() {
  local s="$1"
  s="${s#${s%%[![:space:]]*}}"
  s="${s%${s##*[![:space:]]}}"
  printf '%s' "$s"
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)
      BUILD_DIR="$2"
      shift 2
      ;;
    --rom)
      ROM_PATH="$2"
      shift 2
      ;;
    --providers)
      PROVIDERS="$2"
      shift 2
      ;;
    --prompt)
      PROMPT="$2"
      shift 2
      ;;
    --model)
      MODEL="$2"
      shift 2
      ;;
    --timeout)
      TIMEOUT_SECONDS="$2"
      shift 2
      ;;
    --output)
      OUTPUT_PATH="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage
      exit 2
      ;;
  esac
done

Z3ED="$BUILD_DIR/bin/Debug/z3ed"
if [[ ! -x "$Z3ED" ]]; then
  echo "z3ed not found at $Z3ED" >&2
  echo "Build first: cmake --build $BUILD_DIR --target z3ed --parallel 8" >&2
  exit 2
fi

if [[ -n "$ROM_PATH" && ! -f "$ROM_PATH" ]]; then
  echo "ROM not found: $ROM_PATH" >&2
  exit 2
fi

TIMEOUT_PREFIX=()
if command -v timeout >/dev/null 2>&1; then
  TIMEOUT_PREFIX=(timeout "$TIMEOUT_SECONDS")
elif command -v gtimeout >/dev/null 2>&1; then
  TIMEOUT_PREFIX=(gtimeout "$TIMEOUT_SECONDS")
fi

has_openai_auth() {
  if [[ -n "${OPENAI_API_KEY:-}" ]]; then
    return 0
  fi
  if [[ -n "${OPENAI_BASE_URL:-}" ]]; then
    return 0
  fi
  if [[ -n "${OPENAI_API_BASE:-}" ]]; then
    return 0
  fi
  return 1
}

provider_ready_reason() {
  local provider
  provider="$(echo "$1" | tr '[:upper:]' '[:lower:]')"
  case "$provider" in
    mock)
      echo "ready"
      return 0
      ;;
    gemini)
      if [[ -n "${GEMINI_API_KEY:-}" ]]; then
        echo "ready"
        return 0
      fi
      echo "missing GEMINI_API_KEY"
      return 1
      ;;
    anthropic|claude|sonnet|opus)
      if [[ -n "${ANTHROPIC_API_KEY:-}" ]]; then
        echo "ready"
        return 0
      fi
      echo "missing ANTHROPIC_API_KEY"
      return 1
      ;;
    openai|chatgpt|gpt|lmstudio|lm-studio|custom-openai|openai-compatible)
      if has_openai_auth; then
        echo "ready"
        return 0
      fi
      echo "missing OPENAI_API_KEY/OPENAI_BASE_URL"
      return 1
      ;;
    ollama)
      if [[ -n "${OLLAMA_HOST:-}" ]] || command -v ollama >/dev/null 2>&1; then
        echo "ready"
        return 0
      fi
      echo "missing OLLAMA_HOST and ollama binary"
      return 1
      ;;
    gemini-cli|local-gemini)
      if command -v gemini >/dev/null 2>&1; then
        echo "ready"
        return 0
      fi
      echo "missing gemini CLI"
      return 1
      ;;
    *)
      echo "unknown provider"
      return 1
      ;;
  esac
}

IFS=',' read -r -a PROVIDER_LIST <<<"$PROVIDERS"
RESULTS_TSV="$(mktemp)"
trap 'rm -f "$RESULTS_TSV"' EXIT

PASS=0
FAIL=0
SKIP=0

echo "Running provider matrix smoke with z3ed: $Z3ED"
echo "Providers: $PROVIDERS"

for raw_provider in "${PROVIDER_LIST[@]}"; do
  provider="$(trim "$raw_provider")"
  if [[ -z "$provider" ]]; then
    continue
  fi

  readiness="$(provider_ready_reason "$provider" || true)"
  if [[ "$readiness" != "ready" ]]; then
    SKIP=$((SKIP + 1))
    printf "%s\tskip\t0\t%s\n" "$provider" "$readiness" >> "$RESULTS_TSV"
    printf "SKIP  %-12s %s\n" "$provider" "$readiness"
    continue
  fi

  cmd=("$Z3ED" agent simple-chat "$PROMPT" "--ai_provider=$provider")
  if [[ -n "$ROM_PATH" ]]; then
    cmd+=("--rom=$ROM_PATH")
  else
    cmd+=("--mock-rom")
  fi
  if [[ -n "$MODEL" ]]; then
    cmd+=("--ai_model=$MODEL")
  fi

  output_file="$(mktemp)"
  rc=0
  set +e
  if [[ ${#TIMEOUT_PREFIX[@]} -gt 0 ]]; then
    "${TIMEOUT_PREFIX[@]}" "${cmd[@]}" >"$output_file" 2>&1
    rc=$?
  else
    "${cmd[@]}" >"$output_file" 2>&1
    rc=$?
  fi
  set -e

  if [[ $rc -eq 0 ]]; then
    PASS=$((PASS + 1))
    printf "%s\tpass\t%d\t%s\n" "$provider" "$rc" "ok" >> "$RESULTS_TSV"
    printf "PASS  %-12s ok\n" "$provider"
  else
    FAIL=$((FAIL + 1))
    reason="$(tail -n 3 "$output_file" | tr '\n' ' ' | sed 's/[[:space:]]\+/ /g')"
    printf "%s\tfail\t%d\t%s\n" "$provider" "$rc" "$reason" >> "$RESULTS_TSV"
    printf "FAIL  %-12s rc=%d\n" "$provider" "$rc"
  fi

  rm -f "$output_file"
done

echo ""
echo "Summary: pass=$PASS fail=$FAIL skip=$SKIP"

if [[ -n "$OUTPUT_PATH" ]]; then
  python3 - "$RESULTS_TSV" "$OUTPUT_PATH" <<'PY'
import json
import pathlib
import sys

rows = []
for line in pathlib.Path(sys.argv[1]).read_text().splitlines():
    provider, status, code, reason = line.split("\t", 3)
    rows.append({
        "provider": provider,
        "status": status,
        "exit_code": int(code),
        "reason": reason,
    })

payload = {
    "summary": {
        "pass": sum(1 for r in rows if r["status"] == "pass"),
        "fail": sum(1 for r in rows if r["status"] == "fail"),
        "skip": sum(1 for r in rows if r["status"] == "skip"),
    },
    "results": rows,
}

path = pathlib.Path(sys.argv[2])
path.parent.mkdir(parents=True, exist_ok=True)
path.write_text(json.dumps(payload, indent=2) + "\n")
print(f"Wrote {path}")
PY
fi

if [[ $FAIL -gt 0 ]]; then
  exit 1
fi

exit 0
