#!/usr/bin/env bash
# Verify bounded defaults without launching a real build.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

python3 - "$ROOT_DIR/CMakePresets.json" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as presets_file:
    presets = json.load(presets_file)

build_presets = {
    preset["name"]: preset.get("jobs")
    for preset in presets.get("buildPresets", [])
}
invalid_jobs = {
    name: jobs
    for name, jobs in build_presets.items()
    if isinstance(jobs, bool) or not isinstance(jobs, int) or not 1 <= jobs <= 4
}
if invalid_jobs:
    raise SystemExit(f"build presets must declare one to four workers: {invalid_jobs}")

for name in ("mac-ai", "lin-ai", "win-ai", "mac-test", "lin-test", "win-test"):
    if build_presets.get(name) != 4:
        raise SystemExit(f"build preset {name!r} must default to four workers")
PY

grep -F -- 'cmake --build build --parallel "$BUILD_JOBS"' \
    "$ROOT_DIR/scripts/dev-setup.sh" >/dev/null
grep -F -- 'cmake --build build_ai --target yaze --parallel 4' \
    "$ROOT_DIR/scripts/yaze" >/dev/null
grep -F -- 'cmake --build build_ai --target z3ed --parallel 4' \
    "$ROOT_DIR/scripts/z3ed" >/dev/null
grep -F -- 'cmake --build build_ai --target yaze_test --parallel 4' \
    "$ROOT_DIR/scripts/yaze_test" >/dev/null

TMP_ROOT="$(mktemp -d)"
trap 'rm -rf "$TMP_ROOT"' EXIT
mkdir -p "$TMP_ROOT/bin" "$TMP_ROOT/repo/build_ai"
touch "$TMP_ROOT/repo/CMakePresets.json"

cat >"$TMP_ROOT/bin/cmake" <<'SH'
#!/usr/bin/env bash
printf '%s\n' "$*" >>"$CMAKE_CALL_LOG"
SH
chmod +x "$TMP_ROOT/bin/cmake"

cat >"$TMP_ROOT/bin/ctest" <<'SH'
#!/usr/bin/env bash
exit 0
SH
chmod +x "$TMP_ROOT/bin/ctest"

run_agent_build() {
    local jobs_variable="${1:-}"
    local jobs="${2:-}"
    : >"$TMP_ROOT/cmake.log"
    (
        cd "$TMP_ROOT/repo"
        unset YAZE_BUILD_JOBS CMAKE_BUILD_PARALLEL_LEVEL
        if [[ -n "$jobs_variable" ]]; then
            export "${jobs_variable}=${jobs}"
        fi
        PATH="$TMP_ROOT/bin:$PATH" CMAKE_CALL_LOG="$TMP_ROOT/cmake.log" \
            "$ROOT_DIR/scripts/agent_build.sh" yaze >/dev/null
    )
}

run_agent_build
grep -F -- "--build build_ai --target yaze --parallel 4" "$TMP_ROOT/cmake.log" >/dev/null

run_agent_build YAZE_BUILD_JOBS 7
grep -F -- "--build build_ai --target yaze --parallel 7" "$TMP_ROOT/cmake.log" >/dev/null

run_agent_build CMAKE_BUILD_PARALLEL_LEVEL 6
grep -F -- "--build build_ai --target yaze --parallel 6" "$TMP_ROOT/cmake.log" >/dev/null

run_tests_helper() {
    local jobs="${1:-}"
    : >"$TMP_ROOT/cmake.log"
    (
        cd "$ROOT_DIR"
        unset YAZE_BUILD_JOBS CMAKE_BUILD_PARALLEL_LEVEL
        if [[ -n "$jobs" ]]; then
            export YAZE_BUILD_JOBS="$jobs"
        fi
        PATH="$TMP_ROOT/bin:$PATH" CMAKE_CALL_LOG="$TMP_ROOT/cmake.log" \
            "$ROOT_DIR/scripts/agents/run-tests.sh" mac-dbg >/dev/null
    )
}

run_tests_helper
grep -F -- "--build --preset mac-dbg --parallel 4" "$TMP_ROOT/cmake.log" >/dev/null

run_tests_helper 7
grep -F -- "--build --preset mac-dbg --parallel 7" "$TMP_ROOT/cmake.log" >/dev/null

default_output="$(env -u YAZE_BUILD_JOBS -u CMAKE_BUILD_PARALLEL_LEVEL \
    "$ROOT_DIR/scripts/dev/local-workflow.sh" build --dry-run)"
grep -F -- "Parallel jobs: 4" <<<"$default_output" >/dev/null
grep -F -- "--parallel '4'" <<<"$default_output" >/dev/null

override_output="$("$ROOT_DIR/scripts/dev/local-workflow.sh" build --dry-run --jobs 7)"
grep -F -- "Parallel jobs: 7" <<<"$override_output" >/dev/null
grep -F -- "--parallel '7'" <<<"$override_output" >/dev/null

echo "Build parallelism policy checks passed."
