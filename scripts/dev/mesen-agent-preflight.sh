#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

STATE_PATH=""
ROM_PATH="${ROM_PATH:-Roms/oos168x.sfc}"
META_PATH=""
SCENARIO=""
SCENARIO_MAP=""
OUT_JSON="${OUT_JSON:-.context/runtime/mesen_preflight.json}"
Z3ED_BIN="${Z3ED_BIN:-./build_ai/bin/Debug/z3ed}"
DEFAULT_SCENARIO_MAP="scripts/dev/mesen-state-scenarios.tsv"

usage() {
  cat <<'USAGE'
Usage:
  scripts/dev/mesen-agent-preflight.sh --state <path> [options]

Options:
  --state <path>         Savestate file path (required)
  --rom <path>           ROM path (default: Roms/oos168x.sfc)
  --meta <path>          Metadata path (default: <state>.meta.json)
  --scenario <id>        Expected scenario id (optional)
  --scenario-map <path>  TSV mapping: <state_filename>\t<scenario_id>
  --out <path>           Persist JSON output (default: .context/runtime/mesen_preflight.json)
  --z3ed <path>          z3ed binary path
  -h, --help             Show help

Behavior:
  Runs z3ed mesen-state-hook and emits:
    - One-line hook summary for logs
    - JSON output file for automation consumers
  Exit code is non-zero when freshness check fails.
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --state)
      STATE_PATH="$2"
      shift 2
      ;;
    --rom)
      ROM_PATH="$2"
      shift 2
      ;;
    --meta)
      META_PATH="$2"
      shift 2
      ;;
    --scenario)
      SCENARIO="$2"
      shift 2
      ;;
    --scenario-map)
      SCENARIO_MAP="$2"
      shift 2
      ;;
    --out)
      OUT_JSON="$2"
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
      echo "Unexpected positional argument: $1" >&2
      usage
      exit 1
      ;;
  esac
done

if [[ -z "$STATE_PATH" ]]; then
  echo "--state is required" >&2
  usage
  exit 1
fi
if [[ ! -x "$Z3ED_BIN" ]]; then
  echo "z3ed not executable: $Z3ED_BIN" >&2
  exit 1
fi
if [[ ! -f "$ROM_PATH" ]]; then
  echo "ROM not found: $ROM_PATH" >&2
  exit 1
fi

if [[ -z "$SCENARIO_MAP" && -f "$DEFAULT_SCENARIO_MAP" ]]; then
  SCENARIO_MAP="$DEFAULT_SCENARIO_MAP"
fi

if [[ -z "$SCENARIO" && -n "$SCENARIO_MAP" && -f "$SCENARIO_MAP" ]]; then
  base="$(basename "$STATE_PATH")"
  found="$(awk -F'\t' -v key="$base" '$1==key {print $2; exit}' "$SCENARIO_MAP" || true)"
  if [[ -n "$found" ]]; then
    SCENARIO="$found"
  fi
fi

args=(mesen-state-hook --state "$STATE_PATH" --rom-file "$ROM_PATH" --format=json)
if [[ -n "$META_PATH" ]]; then
  args+=(--meta "$META_PATH")
fi
if [[ -n "$SCENARIO" ]]; then
  args+=(--scenario "$SCENARIO")
fi

mkdir -p "$(dirname "$OUT_JSON")"

tmp_json="$(mktemp)"
set +e
"$Z3ED_BIN" "${args[@]}" >"$tmp_json"
status=$?
set -e

cp "$tmp_json" "$OUT_JSON"

python3 - "$tmp_json" <<'PY'
import json, sys
p = sys.argv[1]
with open(p, 'r', encoding='utf-8') as f:
    data = json.load(f)
summary = data.get("hook_summary", "(no hook summary)")
status = data.get("status", "unknown")
ok = data.get("ok", False)
reasons = data.get("reasons", "")
print(f"[mesen-preflight] status={status} ok={str(ok).lower()} summary={summary}")
if reasons:
    print(f"[mesen-preflight] reasons={reasons}")
PY

rm -f "$tmp_json"
exit $status
