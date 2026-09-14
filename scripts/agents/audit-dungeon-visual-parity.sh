#!/usr/bin/env bash
# Audit dungeon visual parity using the repo's validation ladder.
#
# Tier 1 — Synthetic replay (no ROM): locks C++ draw order against dummy tiles.
# Tier 2 — ROM parser/drawer parity: real tile words from vanilla ROM.
# Tier 3 — Room checksum drift guards: five vanilla room fingerprints.
# Tier 4 — Mesen ROI baselines: rooms 0x007/0x012/0x031/0x065/0x076
#          (needs libpng + canonical US ROM).
# Tier 5 — z3ed bounds audit: dungeon-object-validate mismatch report.
#
# Usage:
#   YAZE_TEST_ROM_VANILLA=$PWD/roms/zelda3.sfc scripts/agents/audit-dungeon-visual-parity.sh
#   scripts/agents/audit-dungeon-visual-parity.sh --config Release --with-validate-report /tmp/report.json
#
# --build-dir DIR selects a configured build; --config NAME is required for
# multi-config builds and must match CMAKE_BUILD_TYPE for single-config builds.
# Without YAZE_TEST_ROM_VANILLA only Tier 1 runs (not a full parity pass).
#
# Canonical US ROM SHA-1 (1 MiB): 6d4f10a8b10e10dbe624cb23cf03b88bb8252973
# Prefer roms/zelda3.sfc, or padded alttp_vanilla.sfc whose first 1 MiB matches.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
# Prefer mac-ai on Darwin; cloud/linux agents use lin-test (Debug layout).
if [[ -z "${YAZE_BUILD_DIR:-}" ]]; then
  if [[ "$(uname -s)" == "Darwin" ]]; then
    BUILD_DIR="$ROOT/build/presets/mac-ai"
  else
    BUILD_DIR="$ROOT/build/presets/lin-test"
  fi
else
  BUILD_DIR="$YAZE_BUILD_DIR"
fi
REPORT_PATH=""
CONFIG=""
MULTI_CONFIG=0

fail() {
  echo "$*" >&2
  exit 1
}

configure_build() {
  [[ -f "$BUILD_DIR/CMakeCache.txt" ]] ||
    fail "Configured build required: $BUILD_DIR/CMakeCache.txt"
  local configurations build_type
  configurations="$(sed -n 's/^CMAKE_CONFIGURATION_TYPES:[^=]*=//p' "$BUILD_DIR/CMakeCache.txt")"
  build_type="$(sed -n 's/^CMAKE_BUILD_TYPE:[^=]*=//p' "$BUILD_DIR/CMakeCache.txt")"
  if [[ -n "$configurations" ]]; then
    MULTI_CONFIG=1
    [[ -n "$CONFIG" ]] || fail "Multi-config build requires --config ($configurations)."
    [[ ";$configurations;" == *";$CONFIG;"* ]] ||
      fail "Unknown configuration '$CONFIG'; available: $configurations"
  elif [[ -n "$CONFIG" && "$CONFIG" != "$build_type" ]]; then
    fail "--config '$CONFIG' does not match CMAKE_BUILD_TYPE='$build_type'."
  else
    CONFIG="$build_type"
  fi
  echo "Build: $BUILD_DIR; configuration: ${CONFIG:-untyped single-config}"
}

build_targets() {
  if [[ -n "$CONFIG" ]]; then
    cmake --build "$BUILD_DIR" --config "$CONFIG" --target "$@" --parallel 4
  else
    cmake --build "$BUILD_DIR" --target "$@" --parallel 4
  fi
}

resolve_test_directory() {
  # Resolve only after building. Never fall back to another configuration, or
  # choose each executable independently from potentially stale output trees.
  if ((MULTI_CONFIG)); then
    TEST_BIN_DIR="$BUILD_DIR/bin/$CONFIG"
    Z3ED_BIN="$TEST_BIN_DIR/z3ed"
  else
    TEST_BIN_DIR=""
    local candidate
    for candidate in "$BUILD_DIR/bin" "$BUILD_DIR/bin/test"; do
      if [[ -x "$candidate/yaze_test_unit" ]]; then
        [[ -z "$TEST_BIN_DIR" ]] || fail "Ambiguous test binaries in bin and bin/test; use a clean build directory."
        TEST_BIN_DIR="$candidate"
      fi
    done
    [[ -n "$TEST_BIN_DIR" ]] || fail "Required test binary not found after build: $BUILD_DIR/bin[/test]/yaze_test_unit"
    Z3ED_BIN="$BUILD_DIR/bin/z3ed"
  fi
  [[ -x "$TEST_BIN_DIR/yaze_test_unit" ]] || fail "Required test binary not found after build: $TEST_BIN_DIR/yaze_test_unit"
  UNIT_BIN="$TEST_BIN_DIR/yaze_test_unit"
  INTEG_BIN="$TEST_BIN_DIR/yaze_test_integration"
  ROM_BIN="$TEST_BIN_DIR/yaze_test_rom_dependent"
  echo "Test binaries: $TEST_BIN_DIR"
}

check_test_evidence() {
  python3 - "$@" <<'PY'
import fnmatch
import json
import sys
import xml.etree.ElementTree as ET

mode, tier, inventory_path, filter_or_report = sys.argv[1:]
try:
    if mode == "discover":
        names = []
        suite = ""
        with open(inventory_path, encoding="utf-8") as stream:
            for raw in stream:
                line = raw.split("#", 1)[0].rstrip()
                if line and not line[0].isspace() and line.endswith("."):
                    suite = line
                elif suite and line.startswith("  ") and line.strip():
                    names.append(suite + line.strip())
                elif line:
                    suite = ""
        positive, _, negative = filter_or_report.partition("-")
        for pattern in positive.split(":"):
            if not any(fnmatch.fnmatchcase(name, pattern) for name in names):
                raise ValueError(f"required test selection is empty: {pattern}")
        if not names or len(names) != len(set(names)):
            raise ValueError("empty or duplicate test inventory")
        if any(not any(fnmatch.fnmatchcase(name, p) for p in positive.split(":"))
               or any(fnmatch.fnmatchcase(name, p) for p in negative.split(":") if p)
               for name in names):
            raise ValueError("test runner did not honor the discovery filter")
        print(f"Tier {tier}: discovered={len(names)}")
        with open(inventory_path + ".json", "w", encoding="utf-8") as stream:
            json.dump(names, stream)
    else:
        with open(inventory_path + ".json", encoding="utf-8") as stream:
            expected = set(json.load(stream))
        cases = ET.parse(filter_or_report).getroot().findall(".//testcase")
        actual = [case.get("classname", "") + "." + case.get("name", "")
                  for case in cases]
        skipped = sum(case.find("skipped") is not None
                      or case.get("status") == "notrun"
                      or case.get("result") in ("skipped", "suppressed")
                      for case in cases)
        failed = sum(case.find("failure") is not None or case.find("error") is not None
                     for case in cases)
        if not cases or set(actual) != expected or len(actual) != len(expected):
            raise ValueError(f"execution does not match discovery: expected={len(expected)}, reported={len(cases)}")
        if skipped or failed:
            raise ValueError(f"executed={len(cases)-skipped}, skipped={skipped}, failed={failed}; not a pass")
        print(f"Tier {tier} PASS: discovered={len(expected)}, executed={len(cases)}, skipped=0")
except (OSError, ValueError, ET.ParseError) as error:
    print(f"Tier {tier} NOT PASSED: {error}", file=sys.stderr)
    raise SystemExit(1)
PY
}

run_test_tier() {
  local tier="$1" binary="$2" filter="$3"
  local inventory="$EVIDENCE_DIR/tier-$tier.list"
  local report="$EVIDENCE_DIR/tier-$tier.xml"
  [[ -x "$binary" ]] || fail "Required Tier $tier binary not found after build: $binary"
  "$binary" --gtest_list_tests --gtest_filter="$filter" >"$inventory"
  check_test_evidence discover "$tier" "$inventory" "$filter"
  "$binary" --gtest_filter="$filter" --gtest_color=no --gtest_output="xml:$report"
  check_test_evidence execute "$tier" "$inventory" "$report"
}

require_clean_validation_report() {
  local report_path="$1"
  if ! command -v python3 >/dev/null 2>&1; then
    echo "python3 is required to validate the Tier 5 JSON report." >&2
    exit 1
  fi

  python3 - "$report_path" <<'PY'
import json
import sys

report_path = sys.argv[1]
try:
    with open(report_path, encoding="utf-8") as report_file:
        report = json.load(report_file)
    summary = report["summary"]
    test_cases = int(summary["test_cases"])
    mismatch_count = int(summary["mismatch_count"])
    empty_traces = int(summary["empty_traces"])
    expected_empty_traces = int(summary["expected_empty_traces"])
except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError) as error:
    print(f"Invalid Tier 5 report {report_path}: {error}", file=sys.stderr)
    raise SystemExit(1)

if test_cases <= 0:
    print(f"Tier 5 report contains no validation cases: {report_path}",
          file=sys.stderr)
    raise SystemExit(1)
if mismatch_count != 0:
    print(f"Tier 5 failed: mismatch_count={mismatch_count} across "
          f"{test_cases} cases ({report_path})", file=sys.stderr)
    raise SystemExit(1)
if empty_traces != expected_empty_traces:
    print(f"Tier 5 failed: empty_traces={empty_traces}, "
          f"expected_empty_traces={expected_empty_traces} ({report_path})",
          file=sys.stderr)
    raise SystemExit(1)

print(f"Tier 5 PASS: mismatch_count=0 across {test_cases} cases; "
      f"empty_traces={empty_traces}")
PY
}

require_distinct_report_path() {
  python3 - "$YAZE_TEST_ROM_VANILLA" "$REPORT_PATH" <<'PY'
import os
import sys

rom_path, report_path = sys.argv[1:]
try:
    aliases_rom = os.path.realpath(rom_path) == os.path.realpath(report_path)
    if os.path.exists(report_path):
        aliases_rom = aliases_rom or os.path.samefile(rom_path, report_path)
except OSError as error:
    print(f"Cannot verify report destination: {error}", file=sys.stderr)
    raise SystemExit(1)
if aliases_rom:
    print("--with-validate-report must not overwrite YAZE_TEST_ROM_VANILLA: "
          + report_path, file=sys.stderr)
    raise SystemExit(1)
PY
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)
      [[ -n "${2:-}" ]] || fail "--build-dir requires a value."
      BUILD_DIR="$2"
      shift 2
      ;;
    --with-validate-report)
      [[ -n "${2:-}" ]] || fail "--with-validate-report requires a value."
      REPORT_PATH="$2"
      shift 2
      ;;
    --config)
      [[ -n "${2:-}" ]] || fail "--config requires a value."
      CONFIG="$2"
      shift 2
      ;;
    -h|--help)
      sed -n '1,24p' "$0"
      exit 0
      ;;
    *)
      echo "Unknown argument: $1" >&2
      exit 2
      ;;
  esac
done

if [[ -n "$REPORT_PATH" && -z "${YAZE_TEST_ROM_VANILLA:-}" ]]; then
  echo "--with-validate-report requires YAZE_TEST_ROM_VANILLA." >&2
  exit 1
fi
[[ ! -d "$REPORT_PATH" ]] || fail "--with-validate-report must name a file, not a directory."
if [[ -n "${YAZE_TEST_ROM_VANILLA:-}" && ! -r "$YAZE_TEST_ROM_VANILLA" ]]; then
  fail "YAZE_TEST_ROM_VANILLA is not readable: $YAZE_TEST_ROM_VANILLA"
fi
command -v python3 >/dev/null 2>&1 || fail "python3 is required to verify test discovery and execution."
if [[ -n "$REPORT_PATH" ]]; then
  require_distinct_report_path
fi
configure_build
EVIDENCE_DIR="$(mktemp -d)"
trap 'rm -rf "$EVIDENCE_DIR"' EXIT
# Inherited sharding/repetition can turn a full audit into a partial run or loop.
unset GTEST_TOTAL_SHARDS GTEST_SHARD_INDEX GTEST_SHARD_STATUS_FILE
export GTEST_REPEAT=1

echo "== Tier 1: synthetic replay + geometry/layer mapping (no ROM) =="
build_targets yaze_test_unit
resolve_test_directory
run_test_tier 1 "$UNIT_BIN" 'DrawRoutineMappingTest.*Plus3*:DrawRoutineMappingTest.*Plus23*:DrawRoutineMappingTest.*Corner*:DrawRoutineMappingTest.*DiagonalCeiling*:DrawRoutineMappingTest.MapsMovingWall*:DrawRoutineMappingTest.Thin*:ObjectDrawerRegistryReplayTest.FloorCopy*:ObjectDrawerRegistryReplayTest.BuiltInWallRoutingAndDiagonalCount*:ObjectDrawerRegistryReplayTest.ConditionalEdgeCaps*:ObjectDrawerRegistryReplayTest.StraightInterroom*:ObjectDrawerRegistryReplayTest.WaterHopStairs*:ObjectDrawerRegistryReplayTest.MovingWalls*:ObjectDrawerRegistryReplayTest.BigHole*:ObjectDrawerRegistryReplayTest.TableRock*:ObjectDrawerRegistryReplayTest.FloodWater*:ObjectDrawerRegistryReplayTest.LongHorizontal*:ObjectDrawerRegistryReplayTest.RightwardsBarUsesUsdasmEndCapsAndRepeatedMiddleColumn:ObjectDrawerRegistryReplayTest.DownwardsBarUsesUsdasmTopThenBodyRows:ObjectDrawerMaskPropagationTest.LaterBG1WriteClearsOnlyItsStreamRevealBit'

if [[ -n "${YAZE_TEST_ROM_VANILLA:-}" ]]; then
  echo
  echo "== Tier 2: vanilla ROM-backed parser/drawer parity =="
  run_test_tier 2 "$UNIT_BIN" 'SupportedRomRoles/RoomObjectRomParityTest.WallCorner*/Vanilla:SupportedRomRoles/RoomObjectRomParityTest.WeirdCorner*/Vanilla:SupportedRomRoles/RoomObjectRomParityTest.FloorCopy*/Vanilla:SupportedRomRoles/RoomObjectRomParityTest.VisualParityGap*/Vanilla:SupportedRomRoles/RoomObjectRomParityTest.BigHole*/Vanilla:SupportedRomRoles/RoomObjectRomParityTest.TableRock*/Vanilla:SupportedRomRoles/RoomObjectRomParityTest.FloodWater*/Vanilla:SupportedRomRoles/RoomObjectRomParityTest.LongRail*/Vanilla'
  build_targets yaze_test_integration yaze_test_rom_dependent
  run_test_tier 2-table "$ROM_BIN" 'DungeonObjectRomValidationTest.TileCountTable_KnownValues'

  echo
  echo "== Tier 3: room fingerprints + structural composition (not independent truth) =="
  # Capture/discovery helpers are opt-in tools, not acceptance tests.
  run_test_tier 3 "$INTEG_BIN" 'DungeonRoomRegressionFixturesTest.*:DungeonRoomRenderParityTest.*-*.DumpBg1OnlyRoomRoiForCapture:*.ScanAllRoomsForFixtureCandidates:*.DiscoverFixtureFingerprints:*IndependentMesen*'
  echo
  echo "== Tier 4: independent Mesen ROI baselines =="
  run_test_tier 4 "$INTEG_BIN" 'DungeonRoomRegressionFixturesTest.*IndependentMesen*'

  if [[ -n "$REPORT_PATH" ]]; then
    echo
    echo "== Tier 5: z3ed dungeon-object-validate bounds audit =="
    build_targets z3ed
    if [[ ! -x "$Z3ED_BIN" ]]; then
      echo "Required Tier 5 binary not found after build: $Z3ED_BIN" >&2
      exit 1
    fi
    "$Z3ED_BIN" dungeon-object-validate \
      --rom "$YAZE_TEST_ROM_VANILLA" \
      --report "$EVIDENCE_DIR/validation.json" \
      --format json
    require_clean_validation_report "$EVIDENCE_DIR/validation.json"
    require_distinct_report_path
    cp "$EVIDENCE_DIR/validation.json" "$REPORT_PATH"
    echo "Wrote validation report to $REPORT_PATH"
  else
    echo "Tier 5 NOT RUN: add --with-validate-report PATH for the bounds audit."
  fi
else
  echo
  echo "Tiers 2/3/4/5 NOT RUN: set YAZE_TEST_ROM_VANILLA to a canonical US ALTTP ROM."
fi

echo
echo "Audit complete. Synthetic replay passing does NOT prove 1:1 emulator parity."
echo "See test/fixtures/visual/dungeon/README.md for independent Mesen baselines."
