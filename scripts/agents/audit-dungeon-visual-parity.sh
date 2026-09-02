#!/usr/bin/env bash
# Audit dungeon visual parity using the repo's validation ladder.
#
# Tier 1 — Synthetic replay (no ROM): locks C++ draw order against dummy tiles.
# Tier 2 — ROM parser/drawer parity: real tile words from vanilla ROM.
# Tier 3 — Room checksum drift guards: five vanilla room fingerprints.
# Tier 4 — Mesen ROI baselines: rooms 0x012 / 0x065 (needs libpng + canonical US ROM).
# Tier 5 — z3ed bounds audit: dungeon-object-validate mismatch report.
#
# Usage:
#   YAZE_TEST_ROM_VANILLA=/path/to/alttp.sfc scripts/agents/audit-dungeon-visual-parity.sh
#   scripts/agents/audit-dungeon-visual-parity.sh --with-validate-report /tmp/report.json

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${YAZE_BUILD_DIR:-$ROOT/build/presets/lin-test}"
UNIT_BIN="$BUILD_DIR/bin/Debug/yaze_test_unit"
INTEG_BIN="$BUILD_DIR/bin/Debug/yaze_test_integration"
Z3ED_BIN="$BUILD_DIR/bin/Debug/z3ed"
REPORT_PATH=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)
      BUILD_DIR="$2"
      UNIT_BIN="$BUILD_DIR/bin/Debug/yaze_test_unit"
      INTEG_BIN="$BUILD_DIR/bin/Debug/yaze_test_integration"
      Z3ED_BIN="$BUILD_DIR/bin/Debug/z3ed"
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

echo "== Tier 1: synthetic replay + mapping (no ROM) =="
cmake --build "$BUILD_DIR" --target yaze_test_unit --parallel 4
"$UNIT_BIN" --gtest_filter='ObjectDrawerRegistryReplayTest.BigHole*:ObjectDrawerRegistryReplayTest.TableRock*:ObjectDrawerRegistryReplayTest.FloodWater*:ObjectDrawerRegistryReplayTest.LongHorizontal*:DrawRoutineMappingTest.*Plus23*'

echo
echo "== Tier 2: ROM-backed parser/drawer parity (skips without ROM) =="
"$UNIT_BIN" --gtest_filter='SupportedRomRoles/RoomObjectRomParityTest.VisualParityGap*:SupportedRomRoles/RoomObjectRomParityTest.BigHole*:SupportedRomRoles/RoomObjectRomParityTest.TableRock*:SupportedRomRoles/RoomObjectRomParityTest.FloodWater*:SupportedRomRoles/RoomObjectRomParityTest.LongRail*'

if [[ -n "${YAZE_TEST_ROM_VANILLA:-}" ]]; then
  echo
  echo "== Tier 3/4: integration room fixtures + Mesen ROI (ROM present) =="
  cmake --build "$BUILD_DIR" --target yaze_test_integration --parallel 4
  YAZE_TEST_ROM_VANILLA="$YAZE_TEST_ROM_VANILLA" \
    "$INTEG_BIN" --gtest_filter='DungeonRoomRegressionFixturesTest.*:DungeonObjectRomValidationTest.TileCountTable_KnownValues'

  if [[ -n "$REPORT_PATH" && -x "$Z3ED_BIN" ]]; then
    echo
    echo "== Tier 5: z3ed dungeon-object-validate bounds audit =="
    "$Z3ED_BIN" dungeon-object-validate \
      --rom "$YAZE_TEST_ROM_VANILLA" \
      --report "$REPORT_PATH" \
      --format json
    echo "Wrote validation report to $REPORT_PATH"
  fi
else
  echo
  echo "Skipping Tier 3/4/5: set YAZE_TEST_ROM_VANILLA to a canonical US ALTTP ROM."
fi

echo
echo "Audit complete. Synthetic replay passing does NOT prove 1:1 emulator parity."
echo "See test/fixtures/visual/dungeon/README.md for independent Mesen baselines."
