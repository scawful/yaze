#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

ROM_PATH="${ROM_PATH:-Roms/oos168x.sfc}"
STATES_DIR=""
SCENARIO_MAP=""
Z3ED_BIN="${Z3ED_BIN:-./build_ai/bin/Debug/z3ed}"
DEFAULT_SCENARIO_MAP="scripts/dev/mesen-state-scenarios.tsv"

usage() {
  cat <<'EOF'
Usage:
  scripts/dev/refresh-oos-states.sh [options] [state1.state state2.state ...]

Options:
  --rom <path>            ROM path for hash pinning (default: Roms/oos168x.sfc)
  --states-dir <path>     Directory to auto-scan for *.state files
  --scenario-map <path>   Optional TSV: <state_filename>\t<scenario_id>
  --z3ed <path>           z3ed binary (default: ./build_ai/bin/Debug/z3ed)
  -h, --help              Show this help

Behavior:
  - Regenerates metadata sidecars via:
      z3ed mesen-state-regen --state <path> --rom-file <path>
  - Verifies each state immediately via:
      z3ed mesen-state-verify --state <path> --rom-file <path>
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --rom)
      ROM_PATH="$2"
      shift 2
      ;;
    --states-dir)
      STATES_DIR="$2"
      shift 2
      ;;
    --scenario-map)
      SCENARIO_MAP="$2"
      shift 2
      ;;
    --z3ed)
      Z3ED_BIN="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    --*)
      echo "Unknown option: $1" >&2
      usage
      exit 1
      ;;
    *)
      break
      ;;
  esac
done

if [[ ! -x "$Z3ED_BIN" ]]; then
  echo "z3ed not executable: $Z3ED_BIN" >&2
  exit 1
fi

if [[ ! -f "$ROM_PATH" ]]; then
  echo "ROM not found: $ROM_PATH" >&2
  exit 1
fi

declare -a STATES=()
if [[ $# -gt 0 ]]; then
  STATES=("$@")
else
  if [[ -z "$STATES_DIR" ]]; then
    if [[ -d "Roms/SaveStates" ]]; then
      STATES_DIR="Roms/SaveStates"
    elif [[ -d "Roms/States" ]]; then
      STATES_DIR="Roms/States"
    else
      echo "No states provided and no default states directory found." >&2
      exit 1
    fi
  fi
  if [[ ! -d "$STATES_DIR" ]]; then
    echo "States directory not found: $STATES_DIR" >&2
    exit 1
  fi
  while IFS= read -r path; do
    STATES+=("$path")
  done < <(find "$STATES_DIR" -type f -name '*.state' | sort)
fi

if [[ ${#STATES[@]} -eq 0 ]]; then
  echo "No .state files found to refresh." >&2
  exit 1
fi

declare -A SCENARIOS=()
if [[ -z "$SCENARIO_MAP" && -f "$DEFAULT_SCENARIO_MAP" ]]; then
  SCENARIO_MAP="$DEFAULT_SCENARIO_MAP"
fi

if [[ -n "$SCENARIO_MAP" ]]; then
  if [[ ! -f "$SCENARIO_MAP" ]]; then
    echo "Scenario map not found: $SCENARIO_MAP" >&2
    exit 1
  fi
  while IFS=$'\t' read -r key value; do
    [[ -z "$key" || "$key" =~ ^# ]] && continue
    SCENARIOS["$key"]="$value"
  done < "$SCENARIO_MAP"
fi

echo "Refreshing ${#STATES[@]} savestate metadata entries..."
echo "ROM: $ROM_PATH"

for state in "${STATES[@]}"; do
  if [[ ! -f "$state" ]]; then
    echo "Skip missing state: $state" >&2
    continue
  fi

  base="$(basename "$state")"
  scenario="${SCENARIOS[$base]:-}"

  regen_args=(mesen-state-regen --state "$state" --rom-file "$ROM_PATH")
  verify_args=(mesen-state-verify --state "$state" --rom-file "$ROM_PATH")
  if [[ -n "$scenario" ]]; then
    regen_args+=(--scenario "$scenario")
    verify_args+=(--scenario "$scenario")
  fi

  echo "-> regen: $state"
  "$Z3ED_BIN" "${regen_args[@]}" >/dev/null

  echo "-> verify: $state"
  "$Z3ED_BIN" "${verify_args[@]}" >/dev/null
done

echo "Done."
