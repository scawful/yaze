#!/usr/bin/env bash
# Verify bounded defaults without launching a real build.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

python3 - "$ROOT_DIR/CMakePresets.json" "$ROOT_DIR" <<'PY'
import json
import re
import shlex
import sys
from pathlib import Path

with open(sys.argv[1], encoding="utf-8") as presets_file:
    presets = json.load(presets_file)
root = Path(sys.argv[2])

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

test_presets = {
    preset["name"]: preset.get("execution", {}).get("jobs")
    for preset in presets.get("testPresets", [])
}
invalid_test_jobs = {
    name: jobs
    for name, jobs in test_presets.items()
    if jobs is not None
    and (isinstance(jobs, bool) or not isinstance(jobs, int) or not 1 <= jobs <= 4)
}
if invalid_test_jobs:
    raise SystemExit(f"test presets may declare at most four workers: {invalid_test_jobs}")

for name in ("fast", "fast-win", "fast-lin"):
    if test_presets.get(name) != 4:
        raise SystemExit(f"test preset {name!r} must default to four workers")


def active_shell_lines(path: Path) -> list[str]:
    """Return logical shell lines, excluding comments and heredoc bodies."""
    lines: list[str] = []
    pending = ""
    heredoc_end: str | None = None
    heredoc_pattern = re.compile(r"<<-?\s*(['\"]?)([A-Za-z_][A-Za-z0-9_]*)\1")
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        stripped = raw_line.strip()
        if heredoc_end is not None:
            if stripped == heredoc_end:
                heredoc_end = None
            continue
        if not stripped or stripped.startswith("#"):
            continue
        heredoc_match = heredoc_pattern.search(raw_line)
        if stripped.endswith("\\"):
            pending += stripped[:-1] + " "
        else:
            lines.append(pending + stripped)
            pending = ""
        if heredoc_match:
            heredoc_end = heredoc_match.group(2)
    if pending:
        lines.append(pending)
    return lines


helper_specs = {
    "scripts/dev-setup.sh": (1, "BUILD_JOBS"),
    "scripts/dev/local_ci.sh": (1, "BUILD_JOBS"),
    "scripts/dev/local_release.sh": (1, "BUILD_JOBS"),
    "scripts/build-wasm.sh": (2, "BUILD_JOBS"),
    "scripts/build_z3ed_wasm.sh": (1, "BUILD_JOBS"),
    "scripts/run_overworld_tests.sh": (1, "BUILD_JOBS"),
    "scripts/install-nightly-local.sh": (1, "nightly_build_jobs"),
    "scripts/install-nightly.sh": (1, "nightly_build_jobs"),
}
assignment_pattern = re.compile(
    r'^(BUILD_JOBS|nightly_build_jobs)="\$\{YAZE_BUILD_JOBS:-'
    r'\$\{CMAKE_BUILD_PARALLEL_LEVEL:-4\}\}"$'
)
for relative_path, (expected_count, jobs_variable) in helper_specs.items():
    path = root / relative_path
    active_lines = active_shell_lines(path)
    assignments = {
        match.group(1)
        for line in active_lines
        if (match := assignment_pattern.fullmatch(line))
    }
    if jobs_variable not in assignments:
        raise SystemExit(f"{relative_path} lacks the bounded environment precedence")

    build_commands: list[list[str]] = []
    for line in active_lines:
        try:
            tokens = shlex.split(line)
        except ValueError as error:
            raise SystemExit(f"cannot parse {relative_path}: {line}: {error}") from error
        for index, token in enumerate(tokens):
            if token == "cmake" and tokens[index + 1 : index + 2] == ["--build"]:
                build_commands.append(tokens[index:])
    if len(build_commands) != expected_count:
        raise SystemExit(
            f"{relative_path} has {len(build_commands)} active cmake builds; "
            f"expected {expected_count}"
        )
    accepted_values = {f"${jobs_variable}", f"${{{jobs_variable}}}"}
    for command in build_commands:
        try:
            parallel_index = command.index("--parallel")
            parallel_value = command[parallel_index + 1].rstrip(";")
        except (ValueError, IndexError) as error:
            raise SystemExit(f"unbounded build in {relative_path}: {' '.join(command)}") from error
        if parallel_value not in accepted_values:
            raise SystemExit(
                f"build in {relative_path} bypasses {jobs_variable}: {' '.join(command)}"
            )
PY

TMP_ROOT="$(mktemp -d)"
trap 'rm -rf "$TMP_ROOT"' EXIT
mkdir -p "$TMP_ROOT/bin" "$TMP_ROOT/repo/build_ai"
touch "$TMP_ROOT/repo/CMakePresets.json"

cat >"$TMP_ROOT/bin/cmake" <<'SH'
#!/usr/bin/env bash
printf '%s\n' "$*" >>"$CMAKE_CALL_LOG"
if [[ "${1:-}" == "--build" && "${2:-}" != --* ]]; then
    build_dir="${2:-build}"
    if [[ "$build_dir" != /* ]]; then
        build_dir="$PWD/$build_dir"
    fi
    mkdir -p "$build_dir/bin"
    touch "$build_dir/bin/z3ed.js"
fi
SH
chmod +x "$TMP_ROOT/bin/cmake"

cat >"$TMP_ROOT/bin/ctest" <<'SH'
#!/usr/bin/env bash
exit 0
SH
chmod +x "$TMP_ROOT/bin/ctest"

cat >"$TMP_ROOT/bin/emcc" <<'SH'
#!/usr/bin/env bash
exit 0
SH
chmod +x "$TMP_ROOT/bin/emcc"

mkdir -p "$TMP_ROOT/wrapper-repo/scripts"
for wrapper in yaze z3ed yaze_test; do
    cp "$ROOT_DIR/scripts/$wrapper" "$TMP_ROOT/wrapper-repo/scripts/$wrapper"
done

assert_missing_binary_help() {
    local wrapper="$1"
    local expected_command="$2"
    local output
    if output="$("$TMP_ROOT/wrapper-repo/scripts/$wrapper" 2>&1)"; then
        echo "$wrapper unexpectedly resolved a binary" >&2
        return 1
    fi
    grep -F -- "$expected_command" <<<"$output" >/dev/null
}

assert_missing_binary_help yaze \
    "cmake --build build_ai --target yaze --parallel 4"
assert_missing_binary_help z3ed \
    "cmake --build build_ai --target z3ed --parallel 4"
assert_missing_binary_help yaze_test \
    "cmake --build build_ai --target yaze_test --parallel 4"

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

mkdir -p "$TMP_ROOT/z3ed-wasm-repo/scripts"
cp "$ROOT_DIR/scripts/build_z3ed_wasm.sh" "$TMP_ROOT/z3ed-wasm-repo/scripts/"

run_z3ed_wasm_helper() {
    local jobs="${1:-}"
    : >"$TMP_ROOT/cmake.log"
    (
        unset YAZE_BUILD_JOBS CMAKE_BUILD_PARALLEL_LEVEL
        if [[ -n "$jobs" ]]; then
            export YAZE_BUILD_JOBS="$jobs"
        fi
        PATH="$TMP_ROOT/bin:$PATH" CMAKE_CALL_LOG="$TMP_ROOT/cmake.log" \
            "$TMP_ROOT/z3ed-wasm-repo/scripts/build_z3ed_wasm.sh" >/dev/null
    )
}

run_z3ed_wasm_helper
grep -F -- "--build $TMP_ROOT/z3ed-wasm-repo/build-wasm --target z3ed --parallel 4" \
    "$TMP_ROOT/cmake.log" >/dev/null

run_z3ed_wasm_helper 7
grep -F -- "--build $TMP_ROOT/z3ed-wasm-repo/build-wasm --target z3ed --parallel 7" \
    "$TMP_ROOT/cmake.log" >/dev/null

mkdir -p "$TMP_ROOT/overworld-repo/scripts" "$TMP_ROOT/overworld-repo/build/bin"
cp "$ROOT_DIR/scripts/run_overworld_tests.sh" "$TMP_ROOT/overworld-repo/scripts/"
touch "$TMP_ROOT/overworld-repo/CMakeLists.txt" "$TMP_ROOT/overworld-repo/test.sfc"

run_overworld_helper() {
    local jobs="${1:-}"
    local output
    local status
    : >"$TMP_ROOT/cmake.log"
    if output="$(
        {
            unset YAZE_BUILD_JOBS CMAKE_BUILD_PARALLEL_LEVEL
            if [[ -n "$jobs" ]]; then
                export YAZE_BUILD_JOBS="$jobs"
            fi
            PATH="$TMP_ROOT/bin:$PATH" CMAKE_CALL_LOG="$TMP_ROOT/cmake.log" \
                "$TMP_ROOT/overworld-repo/scripts/run_overworld_tests.sh" \
                "$TMP_ROOT/overworld-repo/test.sfc" --skip-golden-data \
                --skip-unit-tests --skip-integration --skip-e2e
        } 2>&1
    )"; then
        echo "run_overworld_tests.sh unexpectedly passed with every suite skipped" >&2
        return 1
    else
        status=$?
    fi
    if [[ "$status" -ne 1 ]]; then
        echo "run_overworld_tests.sh exited $status; expected its all-skipped status 1" >&2
        return 1
    fi
    grep -F -- "Golden data extractor built successfully" <<<"$output" >/dev/null
}

run_overworld_helper
grep -F -- "--build $TMP_ROOT/overworld-repo/build --target overworld_golden_data_extractor --parallel 4" \
    "$TMP_ROOT/cmake.log" >/dev/null

run_overworld_helper 7
grep -F -- "--build $TMP_ROOT/overworld-repo/build --target overworld_golden_data_extractor --parallel 7" \
    "$TMP_ROOT/cmake.log" >/dev/null

default_output="$(env -u YAZE_BUILD_JOBS -u CMAKE_BUILD_PARALLEL_LEVEL \
    "$ROOT_DIR/scripts/dev/local-workflow.sh" build --dry-run)"
grep -F -- "Parallel jobs: 4" <<<"$default_output" >/dev/null
grep -F -- "--parallel '4'" <<<"$default_output" >/dev/null

override_output="$("$ROOT_DIR/scripts/dev/local-workflow.sh" build --dry-run --jobs 7)"
grep -F -- "Parallel jobs: 7" <<<"$override_output" >/dev/null
grep -F -- "--parallel '7'" <<<"$override_output" >/dev/null

echo "Build parallelism policy checks passed."
