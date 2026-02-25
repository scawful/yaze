#!/usr/bin/env bash
# oracle_smoke.sh — Oracle-of-Secrets regression smoke checks
#
# Runs three subsystem checks and emits a single JSON summary to stdout.
# Each check targets a focused room subset; no full-ROM scans.
#
# Subsystems:
#   D4 Zora Temple  — structural preflight + required collision rooms 0x25, 0x27
#   D6 Goron Mines  — minecart audit on rooms 0xA8, 0xB8, 0xD8, 0xDA
#   D3 Kalyxo Castle — preflight with required collision room 0x32
#
# Exit codes:
#   0  — ROM not found (skip, not failure) OR structural checks passed
#   1  — z3ed missing, or a structural preflight failure
#
# Notes:
#   - Authoring readiness checks (required rooms for D4/D3) are informational.
#   - They are emitted in JSON output but do not fail the script exit code.
#
# Usage:
#   YAZE_TEST_ROM_OOS=/path/to/oos168.sfc ./scripts/oracle_smoke.sh
#
# Environment:
#   YAZE_TEST_ROM_OOS   Oracle expanded ROM path (preferred)
#   YAZE_TEST_ROM_EXPANDED  Alternate env var
#   Z3ED                Path/command for z3ed (default: ./scripts/z3ed)

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
Z3ED="${Z3ED:-${ROOT}/scripts/z3ed}"
if [[ "$Z3ED" == */* ]]; then
  if [[ ! -x "$Z3ED" ]]; then
    echo "{\"ok\":false,\"status\":\"error\",\"reason\":\"z3ed binary not executable: ${Z3ED}\"}" >&2
    exit 1
  fi
else
  if ! command -v "$Z3ED" >/dev/null 2>&1; then
    echo "{\"ok\":false,\"status\":\"error\",\"reason\":\"z3ed command not found: ${Z3ED}\"}" >&2
    exit 1
  fi
fi

require_subcommand() {
  local cmd="$1"
  if ! "$Z3ED" "$cmd" --help >/dev/null 2>&1; then
    echo "{\"ok\":false,\"status\":\"error\",\"reason\":\"required z3ed subcommand missing: ${cmd}\"}" >&2
    exit 1
  fi
}

require_subcommand "dungeon-oracle-preflight"
require_subcommand "dungeon-minecart-audit"

# --- ROM discovery -----------------------------------------------------------
ROM="${YAZE_TEST_ROM_OOS:-${YAZE_TEST_ROM_EXPANDED:-}}"
if [[ -z "$ROM" ]]; then
  for candidate in \
    "roms/oos168.sfc" "Roms/oos168.sfc" \
    "../roms/oos168.sfc" "../../roms/oos168.sfc" \
    "roms/oos168x.sfc" "Roms/oos168x.sfc"; do
    if [[ -f "$candidate" ]]; then
      ROM="$candidate"
      break
    fi
  done
fi

if [[ -z "$ROM" ]] || [[ ! -f "$ROM" ]]; then
  echo '{"ok":true,"status":"skipped","reason":"Oracle ROM not found. Set YAZE_TEST_ROM_OOS to oos168.sfc path."}'
  exit 0
fi

# --- D4 Zora Temple: structural preflight (fail-fast) ------------------------
d4_structural_ok=true
d4_structural_out=""
if ! d4_structural_out=$("$Z3ED" dungeon-oracle-preflight \
    --rom "$ROM" \
    --format=json 2>/dev/null); then
  d4_structural_ok=false
fi

# --- D4 Zora Temple: required rooms (informational readiness) ----------------
d4_required_rooms_ok=true
d4_required_rooms_out=""
if ! d4_required_rooms_out=$("$Z3ED" dungeon-oracle-preflight \
    --rom "$ROM" \
    --required-collision-rooms=0x25,0x27 \
    --format=json 2>/dev/null); then
  d4_required_rooms_ok=false
fi

# --- D6 Goron Mines: minecart audit (informational; never fails smoke) -------
d6_ok=true
d6_out=""
if ! d6_out=$("$Z3ED" dungeon-minecart-audit \
    --rom "$ROM" \
    --rooms=0xA8,0xB8,0xD8,0xDA \
    --include-track-objects \
    --format=json 2>/dev/null); then
  d6_ok=false
fi

# --- D3 Kalyxo Castle: preflight with prison room requirement ----------------
d3_ok=true
d3_out=""
if ! d3_out=$("$Z3ED" dungeon-oracle-preflight \
    --rom "$ROM" \
    --required-collision-rooms=0x32 \
    --format=json 2>/dev/null); then
  # Non-zero exit means room 0x32 has no authored collision — a known gap,
  # not a structural failure. Treat as informational.
  d3_ok=false
fi

# --- Aggregate and emit -------------------------------------------------------
overall_ok=true
if ! $d4_structural_ok; then
  overall_ok=false
fi

# D4 required-rooms, D6, and D3 non-zero exits are informational; don't fail
# the smoke overall.
# unless we want strict mode.  Uncomment below for strict:
# if ! $d4_required_rooms_ok || ! $d6_ok || ! $d3_ok; then overall_ok=false; fi

status="pass"
if ! $overall_ok; then
  status="fail"
fi

# Produce a single JSON object. Inline sub-objects only if non-empty.
printf '{\n'
printf '  "ok": %s,\n' "$overall_ok"
printf '  "status": "%s",\n' "$status"
printf '  "rom": "%s",\n' "$ROM"
printf '  "checks": {\n'
printf '    "d4_zora_temple": { "structural_ok": %s, "required_rooms_ok": %s, "required_rooms": ["0x25","0x27"] },\n' \
  "$d4_structural_ok" "$d4_required_rooms_ok"
printf '    "d6_goron_mines": { "ok": %s, "rooms": ["0xA8","0xB8","0xD8","0xDA"] },\n' "$d6_ok"
printf '    "d3_kalyxo_castle": { "ok": %s, "required_rooms": ["0x32"] }\n' "$d3_ok"
printf '  }\n'
printf '}\n'

if ! $overall_ok; then
  exit 1
fi
exit 0
