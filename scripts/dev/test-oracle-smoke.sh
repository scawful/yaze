#!/usr/bin/env bash

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SMOKE_SCRIPT="$ROOT/scripts/oracle_smoke.sh"

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

[[ -x "$SMOKE_SCRIPT" ]] || fail "missing executable smoke script: $SMOKE_SCRIPT"

tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

fake_z3ed="$tmp_dir/fake-z3ed"
cat >"$fake_z3ed" <<'EOF_FAKE'
#!/usr/bin/env bash
set -euo pipefail

printf '%s\n' "$*" >>"$CALL_LOG"

cmd="${1:-}"
shift || true
if [[ "${1:-}" == "--help" ]]; then
  printf '{}\n'
  exit 0
fi
args=" $* "
scenario="${SCENARIO:-all_pass}"

emit_preflight() {
  printf '{"Dungeon Oracle Preflight":%s}\n' "$1"
}

case "$cmd" in
  dungeon-oracle-preflight)
    if [[ "$args" == *" --required-water-fill-rooms=0x25,0x27 "* ]]; then
      case "$scenario" in
        membership_missing)
          emit_preflight '{"ok":false,"required_water_fill_rooms_ok":false,"errors":[{"code":"ORACLE_REQUIRED_WATER_FILL_ROOM_MISSING"}]}'
          exit 1
          ;;
        membership_process_poison)
          emit_preflight '{"ok":false,"required_water_fill_rooms_ok":true,"errors":[{"code":"ORACLE_COLLISION_WRITE_REGION_MISSING"}]}'
          exit 1
          ;;
        malformed_membership)
          emit_preflight '{"ok":true,"errors":[]}'
          exit 0
          ;;
        *)
          emit_preflight '{"ok":true,"required_water_fill_rooms_ok":true,"errors":[]}'
          exit 0
          ;;
      esac
    fi

    if [[ "$args" == *" --required-collision-rooms=0x25,0x27 "* ]]; then
      case "$scenario" in
        collision_missing)
          emit_preflight '{"ok":false,"required_rooms_ok":false,"errors":[{"code":"ORACLE_REQUIRED_ROOM_MISSING_COLLISION"}]}'
          exit 1
          ;;
        readiness_poison)
          emit_preflight '{"ok":false,"water_fill_table_ok":false,"required_rooms_ok":true,"errors":[{"code":"ORACLE_WATER_FILL_TABLE_INVALID"}]}'
          exit 1
          ;;
        *)
          emit_preflight '{"ok":true,"required_rooms_ok":true,"errors":[]}'
          exit 0
          ;;
      esac
    fi

    if [[ "$args" == *" --required-collision-rooms=0x32 "* ]]; then
      case "$scenario" in
        d3_missing)
          emit_preflight '{"ok":false,"required_rooms_ok":false,"errors":[{"code":"ORACLE_REQUIRED_ROOM_MISSING_COLLISION"}]}'
          exit 1
          ;;
        readiness_poison)
          emit_preflight '{"ok":false,"water_fill_table_ok":false,"required_rooms_ok":true,"errors":[{"code":"ORACLE_WATER_FILL_TABLE_INVALID"}]}'
          exit 1
          ;;
        *)
          emit_preflight '{"ok":true,"required_rooms_ok":true,"errors":[]}'
          exit 0
          ;;
      esac
    fi

    if [[ "$scenario" == "structural_collision_poison" ]] &&
        [[ "$args" != *" --skip-collision-maps "* ]]; then
      emit_preflight '{"ok":false,"water_fill_table_ok":true,"custom_collision_maps_ok":false,"errors":[{"code":"ORACLE_COLLISION_POINTER_INVALID"}]}'
      exit 1
    fi
    emit_preflight '{"ok":true,"water_fill_table_ok":true,"custom_collision_maps_ok":true,"errors":[]}'
    exit 0
    ;;
  dungeon-minecart-audit)
    printf '%s\n' '{"Dungeon Minecart Audit":{"status":"success","rooms":[]}}'
    exit 0
    ;;
  *)
    exit 2
    ;;
esac
EOF_FAKE
chmod +x "$fake_z3ed"

assert_case() {
  local output_file="$1"
  local call_log="$2"
  local expected_rom="$3"
  local actual_exit="$4"
  local expected_exit="$5"
  local expected_overall="$6"
  local expected_membership="$7"
  local expected_d4_collision="$8"
  local expected_d3_collision="$9"

  python3 - "$output_file" "$call_log" "$expected_rom" "$actual_exit" \
    "$expected_exit" "$expected_overall" "$expected_membership" \
    "$expected_d4_collision" "$expected_d3_collision" <<'PY'
import json
import pathlib
import sys

(
    output_path,
    log_path,
    expected_rom,
    actual_exit,
    expected_exit,
    overall,
    membership,
    d4_collision,
    d3_collision,
) = sys.argv[1:]

def as_bool(value):
    return value == "true"

document = json.loads(pathlib.Path(output_path).read_text())
d4 = document["checks"]["d4_zora_temple"]
d3 = document["checks"]["d3_kalyxo_castle"]

assert int(actual_exit) == int(expected_exit), (actual_exit, expected_exit, document)
assert document["ok"] is as_bool(overall), document
assert document["rom"] == expected_rom, document
assert d4["structural_ok"] is True, d4
assert d4["water_fill_rooms_ok"] is as_bool(membership), d4
assert d4["required_rooms_ok"] is as_bool(d4_collision), d4
assert d3["ok"] is as_bool(d3_collision), d3

calls = pathlib.Path(log_path).read_text().splitlines()
rom_calls = [line for line in calls if " --rom " in f" {line} "]
assert rom_calls, calls
assert all(f"--rom {expected_rom}" in line for line in rom_calls), calls

structural_calls = [
    line for line in rom_calls
    if line.startswith("dungeon-oracle-preflight ") and "--required-" not in line
]
water_fill_calls = [
    line for line in calls
    if "--required-water-fill-rooms=0x25,0x27" in line
]
d4_collision_calls = [
    line for line in calls
    if "--required-collision-rooms=0x25,0x27" in line
]
d3_collision_calls = [
    line for line in calls
    if "--required-collision-rooms=0x32" in line
]
assert len(structural_calls) == 1, calls
assert len(water_fill_calls) == 1, calls
assert len(d4_collision_calls) == 1, calls
assert len(d3_collision_calls) == 1, calls
for focused_call in (
    structural_calls[0],
    water_fill_calls[0],
    d4_collision_calls[0],
    d3_collision_calls[0],
):
    assert "--skip-collision-maps" in focused_call, focused_call
PY
}

run_case() {
  local name="$1"
  local cwd="$2"
  local scenario="$3"
  local expected_rom="$4"
  local expected_exit="$5"
  local expected_overall="$6"
  local expected_membership="$7"
  local expected_d4_collision="$8"
  local expected_d3_collision="$9"
  local call_log="$tmp_dir/$name.calls"
  local output_file="$tmp_dir/$name.json"
  local actual_exit

  if (
    cd "$cwd"
    unset YAZE_TEST_ROM_OOS YAZE_TEST_ROM_EXPANDED \
      YAZE_TEST_ROM_EXPANDED_PATH || true
    CALL_LOG="$call_log" SCENARIO="$scenario" Z3ED="$fake_z3ed" \
      "$SMOKE_SCRIPT"
  ) >"$output_file"; then
    actual_exit=0
  else
    actual_exit=$?
  fi

  assert_case "$output_file" "$call_log" "$expected_rom" "$actual_exit" \
    "$expected_exit" "$expected_overall" "$expected_membership" \
    "$expected_d4_collision" "$expected_d3_collision"
}

simple_case() {
  local scenario="$1"
  local expected_exit="$2"
  local expected_overall="$3"
  local expected_membership="$4"
  local expected_d4_collision="$5"
  local expected_d3_collision="$6"
  local cwd="$tmp_dir/cases/$scenario"

  mkdir -p "$cwd"
  : >"$cwd/oos168x.sfc"
  run_case "$scenario" "$cwd" "$scenario" "oos168x.sfc" "$expected_exit" \
    "$expected_overall" "$expected_membership" "$expected_d4_collision" \
    "$expected_d3_collision"
}

# A base ROM in cwd must not beat a patched ROM in a nested fixture directory.
nested_yaze="$tmp_dir/nested/yaze"
mkdir -p "$nested_yaze/roms"
: >"$nested_yaze/oos168.sfc"
: >"$nested_yaze/roms/oos168x.sfc"
run_case nested "$nested_yaze" collision_missing "roms/oos168x.sfc" \
  0 true true false true

# A base ROM in cwd must not beat a patched ROM in a sibling Oracle checkout.
sibling_root="$tmp_dir/sibling"
sibling_yaze="$sibling_root/yaze"
mkdir -p "$sibling_yaze" "$sibling_root/oracle-of-secrets/Roms"
: >"$sibling_yaze/oos168.sfc"
: >"$sibling_root/oracle-of-secrets/Roms/oos168x.sfc"
run_case sibling "$sibling_yaze" all_pass \
  "../oracle-of-secrets/Roms/oos168x.sfc" 0 true true true true

# WaterFill membership and collision readiness are independent in both
# directions; only membership participates in the default structural result.
simple_case membership_missing 1 false false true true
simple_case collision_missing 0 true true false true

# Aggregate command failures from unrelated checks must not override focused
# JSON fields, and every focused call must skip global collision-map scanning.
simple_case structural_collision_poison 0 true true true true
simple_case readiness_poison 0 true true true true
simple_case membership_process_poison 0 true true true true

# Missing/malformed focused fields fail closed. D3 readiness remains
# informational but must likewise come from its explicit JSON field.
simple_case malformed_membership 1 false false true true
simple_case d3_missing 0 true true true false

echo "PASS: oracle smoke focused-field parsing and patched-first discovery"
