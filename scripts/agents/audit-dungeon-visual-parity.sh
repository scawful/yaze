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
#   scripts/agents/audit-dungeon-visual-parity.sh --with-validate-report /tmp/report.json
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

resolve_bins() {
  # Prefer multi-config Debug outputs (mac-ai Xcode/Ninja multi-config), then
  # single-config bin/test (some Linux presets).
  if [[ -x "$BUILD_DIR/bin/Debug/yaze_test_unit" ]]; then
    UNIT_BIN="$BUILD_DIR/bin/Debug/yaze_test_unit"
    INTEG_BIN="$BUILD_DIR/bin/Debug/yaze_test_integration"
    ROM_BIN="$BUILD_DIR/bin/Debug/yaze_test_rom_dependent"
  elif [[ -x "$BUILD_DIR/bin/test/yaze_test_unit" ]]; then
    UNIT_BIN="$BUILD_DIR/bin/test/yaze_test_unit"
    INTEG_BIN="$BUILD_DIR/bin/test/yaze_test_integration"
    ROM_BIN="$BUILD_DIR/bin/test/yaze_test_rom_dependent"
  else
    UNIT_BIN="$BUILD_DIR/bin/Debug/yaze_test_unit"
    INTEG_BIN="$BUILD_DIR/bin/Debug/yaze_test_integration"
    ROM_BIN="$BUILD_DIR/bin/Debug/yaze_test_rom_dependent"
  fi
  if [[ -x "$BUILD_DIR/bin/Debug/z3ed" ]]; then
    Z3ED_BIN="$BUILD_DIR/bin/Debug/z3ed"
  elif [[ -x "$BUILD_DIR/bin/z3ed" ]]; then
    Z3ED_BIN="$BUILD_DIR/bin/z3ed"
  else
    Z3ED_BIN="$BUILD_DIR/bin/Debug/z3ed"
  fi
}

require_test_suite() {
  local binary="$1"
  local suite="$2"
  if ! "$binary" --gtest_list_tests 2>/dev/null | grep -q "^${suite}\."; then
    echo "Required test suite not found in $binary: $suite" >&2
    exit 1
  fi
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

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)
      BUILD_DIR="$2"
      shift 2
      ;;
    --with-validate-report)
      REPORT_PATH="$2"
      shift 2
      ;;
    -h|--help)
      sed -n '1,20p' "$0"
      exit 0
      ;;
    *)
      echo "Unknown argument: $1" >&2
      exit 2
      ;;
  esac
done

resolve_bins

if [[ -n "$REPORT_PATH" && -z "${YAZE_TEST_ROM_VANILLA:-}" ]]; then
  echo "--with-validate-report requires YAZE_TEST_ROM_VANILLA." >&2
  exit 1
fi

echo "== Tier 1: synthetic replay + geometry/layer mapping (no ROM) =="
cmake --build "$BUILD_DIR" --target yaze_test_unit --parallel 4
"$UNIT_BIN" --gtest_filter='DrawRoutineMappingTest.*Plus3*:DrawRoutineMappingTest.*Plus23*:DrawRoutineMappingTest.*Corner*:DrawRoutineMappingTest.*DiagonalCeiling*:DrawRoutineMappingTest.MapsMovingWall*:ObjectDrawerRegistryReplayTest.FloorCopy*:ObjectDrawerRegistryReplayTest.BuiltInWallRoutingAndDiagonalCount*:ObjectDrawerRegistryReplayTest.ConditionalEdgeCaps*:ObjectDrawerRegistryReplayTest.StraightInterroom*:ObjectDrawerRegistryReplayTest.WaterHopStairs*:ObjectDrawerRegistryReplayTest.MovingWalls*:ObjectDrawerRegistryReplayTest.BigHole*:ObjectDrawerRegistryReplayTest.TableRock*:ObjectDrawerRegistryReplayTest.FloodWater*:ObjectDrawerRegistryReplayTest.LongHorizontal*:ObjectDrawerMaskPropagationTest.LaterBG1WriteClearsOnlyItsStreamRevealBit'

echo
echo "== Tier 2: ROM-backed parser/drawer parity (skips without ROM) =="
"$UNIT_BIN" --gtest_filter='SupportedRomRoles/RoomObjectRomParityTest.WallCorner*:SupportedRomRoles/RoomObjectRomParityTest.WeirdCorner*:SupportedRomRoles/RoomObjectRomParityTest.FloorCopy*:SupportedRomRoles/RoomObjectRomParityTest.VisualParityGap*:SupportedRomRoles/RoomObjectRomParityTest.BigHole*:SupportedRomRoles/RoomObjectRomParityTest.TableRock*:SupportedRomRoles/RoomObjectRomParityTest.FloodWater*:SupportedRomRoles/RoomObjectRomParityTest.LongRail*'

if [[ -n "${YAZE_TEST_ROM_VANILLA:-}" ]]; then
  echo
  echo "== Tier 3/4: integration room fixtures + Mesen ROI (ROM present) =="
  cmake --build "$BUILD_DIR" --target yaze_test_integration \
    yaze_test_rom_dependent --parallel 4
  require_test_suite "$INTEG_BIN" DungeonRoomRegressionFixturesTest
  require_test_suite "$ROM_BIN" DungeonObjectRomValidationTest
  YAZE_TEST_ROM_VANILLA="$YAZE_TEST_ROM_VANILLA" \
    "$INTEG_BIN" \
      --gtest_filter='DungeonRoomRegressionFixturesTest.*:DungeonRoomRenderParityTest.*'
  YAZE_TEST_ROM_VANILLA="$YAZE_TEST_ROM_VANILLA" \
    "$ROM_BIN" \
      --gtest_filter='DungeonObjectRomValidationTest.TileCountTable_KnownValues'

  if [[ -n "$REPORT_PATH" ]]; then
    echo
    echo "== Tier 5: z3ed dungeon-object-validate bounds audit =="
    cmake --build "$BUILD_DIR" --target z3ed --parallel 4
    if [[ ! -x "$Z3ED_BIN" ]]; then
      echo "Required Tier 5 binary not found after build: $Z3ED_BIN" >&2
      exit 1
    fi
    "$Z3ED_BIN" dungeon-object-validate \
      --rom "$YAZE_TEST_ROM_VANILLA" \
      --report "$REPORT_PATH" \
      --format json
    require_clean_validation_report "$REPORT_PATH"
    echo "Wrote validation report to $REPORT_PATH"
  fi
else
  echo
  echo "Skipping Tier 3/4/5: set YAZE_TEST_ROM_VANILLA to a canonical US ALTTP ROM."
fi

echo
echo "Audit complete. Synthetic replay passing does NOT prove 1:1 emulator parity."
echo "See test/fixtures/visual/dungeon/README.md for independent Mesen baselines."
