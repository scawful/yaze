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
cat >"$fake_z3ed" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

printf '%s\n' "$*" >>"$CALL_LOG"

if [[ " $* " == *" --help "* ]]; then
  printf '{}\n'
  exit 0
fi

if [[ "${FAIL_D4_COLLISION:-0}" == "1" ]] &&
    [[ " $* " == *" --required-collision-rooms=0x25,0x27 "* ]]; then
  printf '{}\n'
  exit 1
fi

printf '{}\n'
EOF
chmod +x "$fake_z3ed"

run_smoke() {
  local cwd="$1"
  local call_log="$2"
  local output_file="$3"
  local fail_d4_collision="$4"

  (
    cd "$cwd"
    unset YAZE_TEST_ROM_OOS YAZE_TEST_ROM_EXPANDED || true
    CALL_LOG="$call_log" \
      FAIL_D4_COLLISION="$fail_d4_collision" \
      Z3ED="$fake_z3ed" \
      "$SMOKE_SCRIPT"
  ) >"$output_file"
}

assert_case() {
  local output_file="$1"
  local call_log="$2"
  local expected_rom="$3"
  local expected_collision_ok="$4"

  python3 - "$output_file" "$call_log" "$expected_rom" \
    "$expected_collision_ok" <<'PY'
import json
import pathlib
import sys

output_path, log_path, expected_rom, collision_ok = sys.argv[1:]
document = json.loads(pathlib.Path(output_path).read_text())
d4 = document["checks"]["d4_zora_temple"]

assert document["ok"] is True, document
assert document["rom"] == expected_rom, document
assert d4["structural_ok"] is True, d4
assert d4["water_fill_rooms_ok"] is True, d4
assert d4["required_rooms_ok"] is (collision_ok == "true"), d4

calls = pathlib.Path(log_path).read_text().splitlines()
rom_calls = [line for line in calls if " --rom " in f" {line} "]
assert rom_calls, calls
assert all(f"--rom {expected_rom}" in line for line in rom_calls), calls

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
assert len(water_fill_calls) == 1, calls
assert len(d4_collision_calls) == 1, calls
assert len(d3_collision_calls) == 1, calls
assert "--skip-collision-maps" in water_fill_calls[0], water_fill_calls
assert "--skip-collision-maps" in d4_collision_calls[0], d4_collision_calls
assert "--skip-collision-maps" in d3_collision_calls[0], d3_collision_calls
PY
}

# A base ROM in cwd must not beat a patched ROM in a nested fixture directory.
nested_yaze="$tmp_dir/nested/yaze"
mkdir -p "$nested_yaze/roms"
: >"$nested_yaze/oos168.sfc"
: >"$nested_yaze/roms/oos168x.sfc"
nested_log="$tmp_dir/nested.calls"
nested_output="$tmp_dir/nested.json"
run_smoke "$nested_yaze" "$nested_log" "$nested_output" 1 ||
  fail "collision-readiness failure incorrectly failed structural smoke"
assert_case "$nested_output" "$nested_log" "roms/oos168x.sfc" false

# A base ROM in cwd must not beat a patched ROM in a sibling Oracle checkout.
sibling_root="$tmp_dir/sibling"
sibling_yaze="$sibling_root/yaze"
mkdir -p "$sibling_yaze" "$sibling_root/oracle-of-secrets/Roms"
: >"$sibling_yaze/oos168.sfc"
: >"$sibling_root/oracle-of-secrets/Roms/oos168x.sfc"
sibling_log="$tmp_dir/sibling.calls"
sibling_output="$tmp_dir/sibling.json"
run_smoke "$sibling_yaze" "$sibling_log" "$sibling_output" 0 ||
  fail "sibling patched fixture smoke failed"
assert_case "$sibling_output" "$sibling_log" \
  "../oracle-of-secrets/Roms/oos168x.sfc" true

echo "PASS: oracle smoke readiness separation and patched-first discovery"
