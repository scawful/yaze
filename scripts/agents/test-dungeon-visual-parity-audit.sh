#!/usr/bin/env bash
# Portable audit-runner fixtures; no compiler, application, or real ROM needed.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
AUDIT_SCRIPT="${YAZE_AUDIT_SCRIPT:-$ROOT/scripts/agents/audit-dungeon-visual-parity.sh}"

python3 - "$AUDIT_SCRIPT" <<'PY'
import os
from pathlib import Path
import subprocess
import sys
import tempfile

audit = Path(sys.argv[1]).resolve()
checks = 0

# The same fixture acts as cmake, each test runner, and z3ed. Builds materialize
# binaries only in their configured output directory; stale siblings are traps.
fake_program = r'''#!/usr/bin/env python3
import fnmatch
import json
import os
from pathlib import Path
import shutil
import sys
import xml.etree.ElementTree as ET

args = sys.argv[1:]
name = Path(sys.argv[0]).name
root = Path(os.environ["FIXTURE_ROOT"])
behavior = os.environ.get("FIXTURE_BEHAVIOR", "")
with (root / "calls.log").open("a") as log:
    log.write(str(Path(sys.argv[0])) + " " + " ".join(args) + "\n")
if name == "cmake":
    assert args[0] == "--build"
    build = Path(args[1])
    config = args[args.index("--config") + 1] if "--config" in args else ""
    layout = os.environ["FIXTURE_LAYOUT"]
    if layout == "multi":
        assert config in ("Debug", "Release"), "build needs the requested config"
        directory = build / "bin" / config
    else:
        assert config in ("", os.environ.get("FIXTURE_BUILD_TYPE", "Release"))
        directory = build / "bin" / ("test" if layout == "test" else "")
    targets = args[args.index("--target") + 1:args.index("--parallel")]
    assert args[-1] == "4"
    for target in targets:
        if behavior == "missing-binary" and target == "yaze_test_integration":
            continue
        output_dir = build / "bin" if target == "z3ed" and layout != "multi" else directory
        output_dir.mkdir(parents=True, exist_ok=True)
        shutil.copy2(sys.argv[0], output_dir / target)
    sys.exit(0)
if name == "z3ed":
    if behavior != "stale-report":
        cases = 0 if behavior == "empty-report" else 5
        mismatches = 1 if behavior == "mismatch-report" else 0
        Path(args[args.index("--report") + 1]).write_text(json.dumps({"summary": {
            "test_cases": cases, "mismatch_count": mismatches,
            "empty_traces": 0, "expected_empty_traces": 0}}))
    sys.exit(0)

if name == "yaze_test_unit":
    suites = {
        "DrawRoutineMappingTest": ["APlus3Witness", "APlus23Witness", "ACornerWitness",
                                   "ADiagonalCeilingWitness", "MapsMovingWallWitness"],
        "ObjectDrawerRegistryReplayTest": [s + "Witness" for s in (
            "FloorCopy", "BuiltInWallRoutingAndDiagonalCount", "ConditionalEdgeCaps",
            "StraightInterroom", "WaterHopStairs", "MovingWalls", "BigHole",
            "TableRock", "FloodWater", "LongHorizontal")],
        "ObjectDrawerMaskPropagationTest": ["LaterBG1WriteClearsOnlyItsStreamRevealBit"],
        "SupportedRomRoles/RoomObjectRomParityTest": [s + "Witness/" + role
            for s in ("WallCorner", "WeirdCorner", "FloorCopy", "VisualParityGap",
                      "BigHole", "TableRock", "FloodWater", "LongRail")
            for role in ("Vanilla", "Expanded")],
    }
elif name == "yaze_test_integration":
    suites = {
        "DungeonRoomRegressionFixturesTest": ["PerLayerFingerprintsMatchGolden",
            "Room076WaterOverlayWritesBg2ObjectBuffer",
            "Room012WallRoiMatchesIndependentMesenBaseline",
            "DumpBg1OnlyRoomRoiForCapture", "ScanAllRoomsForFixtureCandidates",
            "DiscoverFixtureFingerprints"],
        "DungeonRoomRenderParityTest": ["Room00FingerprintSmoke"],
    }
else:
    suites = {"DungeonObjectRomValidationTest": ["TileCountTable_KnownValues"]}

pattern = next(a.split("=", 1)[1] for a in args if a.startswith("--gtest_filter="))
positive, _, negative = pattern.partition("-")
selected = [(suite, test) for suite, tests in suites.items() for test in tests
    if any(fnmatch.fnmatchcase(suite + "." + test, p) for p in positive.split(":"))
    and not any(fnmatch.fnmatchcase(suite + "." + test, p) for p in negative.split(":") if p)]
if behavior == "empty-discovery":
    selected = []
elif behavior == "missing-suite":
    selected = [(s, t) for s, t in selected if s != "ObjectDrawerMaskPropagationTest"]
if "--gtest_list_tests" in args:
    for suite in sorted({s for s, _ in selected}):
        print(suite + ".")
        for s, test in selected:
            if s == suite:
                print("  " + test + "  # fixture parameter comment")
    sys.exit(0)
if behavior == "zero-execution":
    selected = []
elif behavior == "partial-execution":
    selected = selected[:-1]
if behavior == "nonzero-exit":
    sys.exit(7)
report = ET.Element("testsuites")
suite_node = ET.SubElement(report, "testsuite")
for suite, test in selected:
    case = ET.SubElement(suite_node, "testcase", classname=suite, name=test,
                         status="run", result="completed")
    # Expanded ROMs and capture tools are intentionally unavailable. Production
    # filters must avoid them, not quietly accept their skipped results.
    if (test.endswith("/Expanded") or test.startswith(("Dump", "Scan", "Discover"))
            or behavior == "skip-all"
            or behavior == "skip-mesen" and "IndependentMesen" in test):
        ET.SubElement(case, "skipped")
    if behavior == "xml-failure":
        ET.SubElement(case, "failure")
output = next((a.split("xml:", 1)[1] for a in args if a.startswith("--gtest_output=xml:")), None)
if output and behavior != "missing-xml":
    ET.ElementTree(report).write(output)
print("[  PASSED  ] fixture runner exit (not evidence by itself)")
'''

with tempfile.TemporaryDirectory(prefix="yaze-parity-audit-") as temporary:
    root = Path(temporary)
    fake_bin = root / "fake-bin"
    fake_bin.mkdir()
    cmake = fake_bin / "cmake"
    cmake.write_text(fake_program)
    cmake.chmod(0o755)
    rom = root / "fixture.sfc"
    rom.write_bytes(b"mock ROM: never parsed by real code")

    def run(label, *, layout="bin", config=None, rom_present=False,
            behavior="", success=True, expected="", stale=False,
            report=False, ambiguous=False, rom_missing=False):
        global checks
        case_root = root / str(checks)
        build = case_root / "build with spaces"
        build.mkdir(parents=True)
        cache = "CMAKE_CONFIGURATION_TYPES:STRING=Debug;Release\n" if layout == "multi" else "CMAKE_BUILD_TYPE:STRING=Release\n"
        (build / "CMakeCache.txt").write_text(cache)
        if stale:
            trap_dir = build / "bin" / ("Debug" if config == "Release" else "Release")
            trap_dir.mkdir(parents=True)
            for name in ("yaze_test_unit", "yaze_test_integration", "yaze_test_rom_dependent", "z3ed"):
                trap = trap_dir / name
                trap.write_text("#!/usr/bin/env bash\necho STALE-BINARY >&2\nexit 9\n")
                trap.chmod(0o755)
        if ambiguous:
            second = build / "bin/test/yaze_test_unit"
            second.parent.mkdir(parents=True)
            second.write_text(fake_program)
            second.chmod(0o755)
        env = {k: v for k, v in os.environ.items()
               if not k.startswith(("YAZE_TEST_ROM", "GTEST_")) and k != "YAZE_SKIP_ROM_TESTS"}
        env.update(PATH=str(fake_bin) + os.pathsep + env["PATH"], FIXTURE_ROOT=str(case_root),
                   FIXTURE_LAYOUT=layout, FIXTURE_BEHAVIOR=behavior)
        if rom_present or rom_missing:
            env["YAZE_TEST_ROM_VANILLA"] = str(root / "absent.sfc" if rom_missing else rom)
        command = ["bash", str(audit), "--build-dir", str(build)]
        if config:
            command += ["--config", config]
        report_path = case_root / "report.json"
        if report:
            # A pre-existing green report must not rescue a no-output CLI run.
            report_path.write_text('{"summary":{"test_cases":5,"mismatch_count":0,"empty_traces":0,"expected_empty_traces":0}}')
            command += ["--with-validate-report", str(report_path)]
        result = subprocess.run(command, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if (result.returncode == 0) != success or expected not in result.stdout:
            raise AssertionError(f"{label}: exit={result.returncode}, expected success={success}, text={expected!r}\n{result.stdout}")
        calls = (case_root / "calls.log").read_text() if (case_root / "calls.log").exists() else ""
        if success:
            assert "STALE-BINARY" not in result.stdout
            assert "Tier 1 PASS: discovered=16, executed=16, skipped=0" in result.stdout
            if rom_present:
                assert "Tier 2 PASS: discovered=8, executed=8, skipped=0" in result.stdout
                assert "Tier 3 PASS: discovered=3, executed=3, skipped=0" in result.stdout
                assert "Tier 4 PASS: discovered=1, executed=1, skipped=0" in result.stdout
            else:
                assert "--gtest_filter=SupportedRomRoles" not in calls
            if config:
                for call in calls.splitlines():
                    if "cmake --build" in call:
                        assert "--config " + config in call
        checks += 1
        print(f"PASS: {label}")

    run("single-config bin, created by build", expected="Tiers 2/3/4/5 NOT RUN")
    run("single-config bin/test", layout="test", config="Release", rom_present=True, report=True)
    run("single-config ignores stale multi-config siblings", stale=True, rom_present=True)
    for config in ("Debug", "Release"):
        run("multi-config " + config, layout="multi", config=config, rom_present=True, stale=True, report=True)
    run("multi-config requires explicit selection", layout="multi", success=False, expected="requires --config")
    run("single-config mismatch rejected", config="Debug", success=False, expected="does not match CMAKE_BUILD_TYPE")
    run("unknown multi-config rejected", layout="multi", config="Missing", success=False, expected="Unknown configuration")
    run("ambiguous output layout rejected", ambiguous=True, success=False, expected="Ambiguous test binaries")
    run("missing ROM rejected before execution", rom_missing=True, success=False, expected="not readable")
    run("report requires ROM", report=True, success=False, expected="requires YAZE_TEST_ROM_VANILLA")
    for behavior, expected in (
            ("empty-discovery", "required test selection is empty"),
            ("missing-suite", "required test selection is empty"),
            ("zero-execution", "execution does not match discovery"),
            ("partial-execution", "execution does not match discovery"),
            ("skip-all", "skipped=16"), ("skip-mesen", "Tier 4 NOT PASSED"),
            ("xml-failure", "failed=16"), ("missing-xml", "Tier 1 NOT PASSED"),
            ("nonzero-exit", "Tier 1: discovered=16"),
            ("missing-binary", "Required Tier 3 binary not found"),
            ("stale-report", "Invalid Tier 5 report"),
            ("empty-report", "contains no validation cases"),
            ("mismatch-report", "mismatch_count=1")):
        run(behavior, behavior=behavior, rom_present=True, report=True,
            success=False, expected=expected)

print(f"PASS: {checks} dungeon parity audit fixtures (no real builds or ROMs)")
PY
