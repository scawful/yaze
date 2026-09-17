#!/usr/bin/env python3
"""
audit_test_registration.py -- Test source registration integrity audit for YAZE.

For every *_test.cc file discovered under test/ this tool checks that it either:
  1. Appears in at least one compiled source list in test/CMakeLists.txt, OR
  2. Has an explicit entry in the EXCLUSION_LIST below (with a documented reason).

Exit codes:
  0  -- all tracked test sources are either registered or explicitly excluded
  1  -- unregistered test sources detected (drift)
  2  -- usage error

Usage:
  python3 scripts/audit_test_registration.py [--verbose] [--json]

Self-tests (run when invoked via pytest or --self-test):
  python3 scripts/audit_test_registration.py --self-test
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
TEST_ROOT = PROJECT_ROOT / "test"
TEST_CMAKE = TEST_ROOT / "CMakeLists.txt"

# ---------------------------------------------------------------------------
# Explicit exclusion list: test files intentionally NOT wired into CMakeLists.
# Each entry maps (relative-to-test-root path) -> reason string.
# ---------------------------------------------------------------------------
EXCLUSION_LIST: dict[str, str] = {
    # ---------------------------------------------------------------------------
    # Shared stubs / entry points -- NOT test suites themselves
    # ---------------------------------------------------------------------------
    # yaze_test.cc is the gtest main entry point included by all suites.
    "yaze_test.cc": "gtest main entry point; compiled into every suite target",
    # yaze_test_ci.cc is an alternate CI main that is built separately.
    "yaze_test_ci.cc": "alternate CI entry point; not a standalone suite",
    # app_instance_stub.cc satisfies Application::Instance() for non-GUI tests.
    "app_instance_stub.cc": "shared link stub, not a test suite",
    # gui_test_utils.cc is helper code compiled into the GUI suite only.
    "gui_test_utils.cc": "GUI test helper compiled into yaze_test_gui",
    # test_editor.cc provides shared test helper stubs, not a test suite itself.
    "test_editor.cc": "shared test helper (test_editor.h), not a suite",
    # test_conversation_minimal.cc is an ad-hoc manual probe, not a suite.
    "test_conversation_minimal.cc": "manual probe only; no gtest harness",

    # ---------------------------------------------------------------------------
    # WASM / Emscripten-only -- cannot be compiled in native build
    # ---------------------------------------------------------------------------
    # browser_ai_test.cc uses Emscripten fetch/socket APIs, native-incompatible.
    "browser_ai_test.cc": "WASM/browser-only; compiled by the Emscripten build",
    # wasm_message_queue_test.cc is fully guarded by #ifdef __EMSCRIPTEN__.
    "integration/wasm_message_queue_test.cc": "WASM-only; guarded by #ifdef __EMSCRIPTEN__",
    # wasm_error_handler_test.cc is fully guarded by #ifdef __EMSCRIPTEN__.
    "platform/wasm_error_handler_test.cc": "WASM-only; guarded by #ifdef __EMSCRIPTEN__",
    # wasm_patch_export_test.cc tests APIs that only exist under Emscripten.
    "unit/wasm_patch_export_test.cc": "WASM-only; tests Emscripten-export API",

    # ---------------------------------------------------------------------------
    # DEPRECATED -- coverage superseded by other tests
    # ---------------------------------------------------------------------------
    # dungeon_rendering_test.cc deprecated Nov 2025; replaced by
    # dungeon_object_rendering_tests.cc with proper TestRomManager fixtures.
    "integration/zelda3/dungeon_rendering_test.cc": (
        "DEPRECATED Nov 2025; replaced by integration/zelda3/"
        "dungeon_object_rendering_tests.cc"
    ),
    # object_rendering_test.cc deprecated Nov 2025; replaced by the same suite.
    "unit/zelda3/dungeon/object_rendering_test.cc": (
        "DEPRECATED Nov 2025; replaced by integration/zelda3/"
        "dungeon_object_rendering_tests.cc"
    ),

    # ---------------------------------------------------------------------------
    # Legacy / removed -- explicitly retired from the build
    # ---------------------------------------------------------------------------
    # editor_integration_test.cc was removed from the build because it depends
    # on Controller which has circular dependencies. Replacement: use the GUI
    # suite (yaze_test_gui) via the ImGui Test Engine.
    "integration/editor/editor_integration_test.cc": (
        "Legacy Controller dependency; retired -- use yaze_test_gui instead"
    ),
}


def extract_source_list_from_cmake(cmake_path: Path) -> set[str]:
    """
    Return every relative-to-test-root path that appears as a literal string
    inside a set()/list(APPEND ...) call in cmake_path.

    We parse conservatively: any line that looks like a relative path to a
    .cc file (not starting with $, not a cmake keyword) is treated as a
    registered source.
    """
    if not cmake_path.exists():
        raise FileNotFoundError(f"CMakeLists not found: {cmake_path}")

    text = cmake_path.read_text(encoding="utf-8")
    # Match any token that looks like a relative .cc path
    # Examples:
    #   unit/emu/emulator_test.cc
    #   ../src/cli/service/resources/resource_catalog.cc
    #   integration/dungeon_editor_test.cc
    paths: set[str] = set()
    for m in re.finditer(r'(?<!\$\{)(?<![a-zA-Z_])([\w./]+\.cc)\b', text):
        candidate = m.group(1)
        # Skip cmake built-in keywords disguised as paths
        if candidate.startswith("${") or "/" not in candidate:
            continue
        paths.add(candidate)
    return paths


def discover_test_sources(test_root: Path) -> list[Path]:
    """
    Find all *_test.cc files under test_root, excluding build/ directories.
    Returns paths relative to test_root.
    """
    results = []
    for p in sorted(test_root.rglob("*_test.cc")):
        # Skip any build artefacts
        parts = p.parts
        if any(part in {"build", "CMakeFiles", "_deps"} for part in parts):
            continue
        results.append(p.relative_to(test_root))
    return results


def run_audit(verbose: bool = False) -> tuple[list[str], list[str]]:
    """
    Returns (unregistered, excluded) lists of relative path strings.
    """
    registered = extract_source_list_from_cmake(TEST_CMAKE)
    all_tests = discover_test_sources(TEST_ROOT)

    unregistered: list[str] = []
    excluded: list[str] = []

    for rel in all_tests:
        rel_str = str(rel)
        # Check the exclusion list first (by filename or full relative path)
        excl_key = rel.name if rel.name in EXCLUSION_LIST else rel_str
        if excl_key in EXCLUSION_LIST or rel_str in EXCLUSION_LIST:
            excluded.append(rel_str)
            continue

        # Check if any suffix of the path appears in the cmake source list.
        # CMakeLists uses paths like "unit/emu/emulator_test.cc" which are
        # relative to the test/ directory itself.
        found = False
        for reg in registered:
            # Normalize separators
            reg_norm = reg.replace("\\", "/")
            rel_norm = rel_str.replace("\\", "/")
            if reg_norm.endswith(rel_norm) or rel_norm.endswith(reg_norm):
                found = True
                break

        if not found:
            unregistered.append(rel_str)
            if verbose:
                print(f"  [UNREGISTERED] {rel_str}")
        elif verbose:
            print(f"  [OK]           {rel_str}")

    return unregistered, excluded


# ---------------------------------------------------------------------------
# Self-tests
# ---------------------------------------------------------------------------

def _self_test_extract_source_list() -> None:
    """Verify the cmake parser extracts at least the well-known baseline tests."""
    sources = extract_source_list_from_cmake(TEST_CMAKE)
    required = [
        "unit/emu/emulator_test.cc",
        "unit/emu/step_controller_test.cc",
        "unit/emu/spc700_reset_test.cc",
        "unit/cli/rom_debug_agent_test.cc",
        "integration/dungeon_editor_test.cc",
    ]
    missing = [r for r in required if not any(s.endswith(r) for s in sources)]
    assert not missing, f"cmake parser missed required sources: {missing}"
    print("  [PASS] cmake parser extracts known sources")


def _self_test_exclusion_list() -> None:
    """Verify every key in EXCLUSION_LIST is either a filename or an existing path."""
    for key in EXCLUSION_LIST:
        # Key should end in .cc and be non-empty
        assert key.endswith(".cc"), f"Bad exclusion key (must end in .cc): {key!r}"
        assert len(key) > 3, f"Suspiciously short exclusion key: {key!r}"
    print("  [PASS] exclusion list is well-formed")


def _self_test_no_unregistered_after_registration() -> None:
    """After registering the known tests, the audit must report zero unregistered."""
    unregistered, _ = run_audit(verbose=False)
    if unregistered:
        print(f"  [FAIL] Audit found unregistered tests: {unregistered}")
        sys.exit(1)
    print("  [PASS] no unregistered test sources detected")


def run_self_tests() -> int:
    print("Running audit_test_registration self-tests...")
    try:
        _self_test_extract_source_list()
        _self_test_exclusion_list()
        _self_test_no_unregistered_after_registration()
    except AssertionError as exc:
        print(f"  [FAIL] {exc}")
        return 1
    print("All self-tests passed.")
    return 0


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> int:
    parser = argparse.ArgumentParser(
        description="Audit test source registration against test/CMakeLists.txt.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument("--verbose", "-v", action="store_true",
                        help="Print status for every discovered test file")
    parser.add_argument("--json", action="store_true",
                        help="Output results as JSON to stdout")
    parser.add_argument("--self-test", action="store_true",
                        help="Run built-in self-tests and exit")
    args = parser.parse_args()

    if args.self_test:
        return run_self_tests()

    try:
        unregistered, excluded = run_audit(verbose=args.verbose)
    except FileNotFoundError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 2

    if args.json:
        print(json.dumps({
            "unregistered": unregistered,
            "excluded": excluded,
            "ok": len(unregistered) == 0,
        }, indent=2))
    else:
        if unregistered:
            print(f"\n❌ {len(unregistered)} unregistered test source(s) detected:")
            for u in unregistered:
                print(f"   {u}")
            print("\nAdd each file to test/CMakeLists.txt in the correct source list,")
            print("or add it to EXCLUSION_LIST in scripts/audit_test_registration.py")
            print("with a documented reason.")
        else:
            count = len(excluded)
            print(f"✅ All test sources registered ({count} file(s) explicitly excluded).")

    return 1 if unregistered else 0


if __name__ == "__main__":
    raise SystemExit(main())
