#!/usr/bin/env bash
# Hermetic contract tests for the quality tooling entry points:
# scripts/lib/clang_tools.sh discovery, scripts/quality_check.sh advisory/gate
# semantics, and scripts/lint.sh as the changed-file fast path.
#
# clang-format, clang-tidy, and cppcheck are stubbed through the YAZE_CLANG_*
# and YAZE_CPPCHECK overrides, so nothing here formats or analyses real sources.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
QUALITY_CHECK="${REPO_ROOT}/scripts/quality_check.sh"
LINT="${REPO_ROOT}/scripts/lint.sh"

WORK_DIR="$(mktemp -d)"
trap 'rm -rf "${WORK_DIR}"' EXIT

failures=0

fail() {
  echo "quality tooling test failed: $*" >&2
  failures=$((failures + 1))
}

# Stub binaries record their arguments so tests can assert on what ran.
make_stub() {
  local name="$1" exit_code="$2" stderr_line="${3:-}"
  local path="${WORK_DIR}/${name}"
  cat >"$path" <<EOF
#!/usr/bin/env bash
if [[ "\${1:-}" == "--version" ]]; then
  echo "${name} version 22.1.5"
  exit 0
fi
printf '%s\n' "\$@" >>"${WORK_DIR}/${name}.args"
[[ -z "${stderr_line}" ]] || echo "${stderr_line}" >&2
exit ${exit_code}
EOF
  chmod +x "$path"
  printf '%s\n' "$path"
}

CLEAN_FORMAT="$(make_stub clang-format-clean 0)"
# clang-format reports violations on stderr and exits non-zero under --Werror.
DIRTY_FORMAT="$(make_stub clang-format-dirty 1 \
  'src/app/example.cc:1:1: error: code should be clang-formatted [-Wclang-format-violations]')"
CLEAN_CPPCHECK="$(make_stub cppcheck-clean 0)"

# --- scripts/lib/clang_tools.sh ---------------------------------------------

# shellcheck source=scripts/lib/clang_tools.sh
source "${REPO_ROOT}/scripts/lib/clang_tools.sh"

pinned_version="$(yaze_clang_pinned_version)" ||
  fail "no version pin readable from .clang-format-version"
[[ "$pinned_version" =~ ^[0-9]+(\.[0-9]+)*$ ]] ||
  fail ".clang-format-version holds '${pinned_version}', expected a bare version"

pinned_major="$(yaze_clang_pinned_major)"
[[ "$pinned_major" == "${pinned_version%%.*}" ]] ||
  fail "pinned major '${pinned_major}' does not match '${pinned_version}'"

# The pin has to stay in lockstep with the places that install clang-format.
grep -q "clang-format==\$(cat .clang-format-version)" \
  "${REPO_ROOT}/.github/workflows/ci.yml" ||
  fail "CI no longer installs clang-format from .clang-format-version"

grep -q "rev: v${pinned_version}\$" "${REPO_ROOT}/.pre-commit-config.yaml" ||
  fail ".pre-commit-config.yaml does not pin mirrors-clang-format to v${pinned_version}"

[[ "$(YAZE_CLANG_FORMAT="$CLEAN_FORMAT" yaze_find_clang_format)" == "$CLEAN_FORMAT" ]] ||
  fail "YAZE_CLANG_FORMAT override ignored"

status=0
YAZE_CLANG_TIDY="" yaze_find_clang_tidy >/dev/null || status=$?
[[ "$status" -eq 1 ]] ||
  fail "empty YAZE_CLANG_TIDY returned ${status}, expected 1 (tool missing)"

# A mismatched major warns but never fails: contributors on another release
# still get a usable tool.
mismatched="$(make_stub clang-format-legacy 0)"
sed -i.bak "s/version 22.1.5/version 18.1.8/" "$mismatched" && rm -f "${mismatched}.bak"
warning="$(yaze_warn_clang_version "$mismatched" clang-format 2>&1 >/dev/null)"
status=$?
[[ "$status" -eq 0 ]] || fail "version mismatch returned ${status}, expected 0"
[[ "$warning" == *"major 18"* && "$warning" == *"major ${pinned_major}"* ]] ||
  fail "version mismatch warning missing majors: '${warning}'"

[[ -z "$(yaze_warn_clang_version "$CLEAN_FORMAT" clang-format 2>&1 >/dev/null)" ]] ||
  fail "matching major should not warn"

# --- scripts/quality_check.sh ------------------------------------------------

help_output="$(bash "$QUALITY_CHECK" --help)" ||
  fail "--help exited non-zero"
[[ "$help_output" == *"--advisory"* && "$help_output" == *"--gate"* ]] ||
  fail "--help does not document both modes"
[[ "$help_output" != *"set -uo pipefail"* ]] ||
  fail "--help printed script code"

status=0
bash "$QUALITY_CHECK" --bogus >/dev/null 2>&1 || status=$?
[[ "$status" -eq 2 ]] || fail "unknown option returned ${status}, expected 2"

run_quality_check() {
  YAZE_CLANG_FORMAT="$1" YAZE_CPPCHECK="$2" bash "$QUALITY_CHECK" "${@:3}"
}

status=0
advisory_output="$(run_quality_check "$DIRTY_FORMAT" "$CLEAN_CPPCHECK" 2>&1)" || status=$?
[[ "$status" -eq 0 ]] ||
  fail "advisory mode returned ${status} with findings, expected 0"
[[ "$advisory_output" == *"Advisory mode"* ]] ||
  fail "advisory mode did not say it was suppressing the failure"

status=0
gate_output="$(run_quality_check "$DIRTY_FORMAT" "$CLEAN_CPPCHECK" --gate 2>&1)" || status=$?
[[ "$status" -eq 1 ]] ||
  fail "gate mode returned ${status} with formatting violations, expected 1"
[[ "$gate_output" == *"clang-format"* ]] ||
  fail "gate output does not name the failing check"

status=0
run_quality_check "$CLEAN_FORMAT" "$CLEAN_CPPCHECK" --gate >/dev/null 2>&1 || status=$?
[[ "$status" -eq 0 ]] || fail "gate mode returned ${status} with no findings, expected 0"

# A gate that silently skips a missing tool is not a gate.
status=0
YAZE_CLANG_FORMAT="" YAZE_CPPCHECK="$CLEAN_CPPCHECK" \
  bash "$QUALITY_CHECK" --gate >/dev/null 2>&1 || status=$?
[[ "$status" -eq 3 ]] || fail "gate mode with no clang-format returned ${status}, expected 3"

status=0
YAZE_CLANG_FORMAT="" YAZE_CPPCHECK="$CLEAN_CPPCHECK" \
  bash "$QUALITY_CHECK" >/dev/null 2>&1 || status=$?
[[ "$status" -eq 0 ]] || fail "advisory mode with no clang-format returned ${status}, expected 0"

# --- scripts/lint.sh ---------------------------------------------------------

# The fast path must check exactly the files it is given, and must not fall back
# to the whole tree.
rm -f "${WORK_DIR}/clang-format-clean.args"
status=0
YAZE_CLANG_FORMAT="$CLEAN_FORMAT" YAZE_CLANG_TIDY="" \
  bash "$LINT" check src/rom/rom.cc >/dev/null 2>&1 || status=$?
[[ "$status" -eq 0 ]] || fail "lint.sh check on one file returned ${status}, expected 0"
checked="$(grep -cv '^--' "${WORK_DIR}/clang-format-clean.args")"
[[ "$checked" -eq 1 ]] || fail "lint.sh checked ${checked} paths, expected only the named one"
grep -qx 'src/rom/rom.cc' "${WORK_DIR}/clang-format-clean.args" ||
  fail "lint.sh did not check the file it was given"
grep -qx -- '--style=file' "${WORK_DIR}/clang-format-clean.args" ||
  fail "lint.sh did not pass --style=file"

status=0
YAZE_CLANG_FORMAT="$DIRTY_FORMAT" YAZE_CLANG_TIDY="" \
  bash "$LINT" check src/rom/rom.cc >/dev/null 2>&1 || status=$?
[[ "$status" -eq 1 ]] || fail "lint.sh returned ${status} on a format violation, expected 1"

# Missing clang-format is a clean error, not a set -e abort mid-script.
missing_output="$(YAZE_CLANG_FORMAT="" YAZE_CLANG_TIDY="" \
  bash "$LINT" check src/rom/rom.cc 2>&1)"
status=$?
[[ "$status" -eq 1 ]] || fail "lint.sh returned ${status} without clang-format, expected 1"
[[ "$missing_output" == *"clang-format not found"* ]] ||
  fail "lint.sh did not explain the missing tool: '${missing_output}'"

# Required tidy coverage is tested in a tiny isolated project. This must not
# depend on whether the real checkout has a root compile database symlink.
SCOPED_ROOT="${WORK_DIR}/scoped project"
SCOPED_BUILD="${SCOPED_ROOT}/build with spaces"
mkdir -p "${SCOPED_ROOT}/scripts/lib" "${SCOPED_ROOT}/src" "$SCOPED_BUILD"
cp "$LINT" "${SCOPED_ROOT}/scripts/lint.sh"
cp "${REPO_ROOT}/scripts/lib/clang_tools.sh" "${SCOPED_ROOT}/scripts/lib/clang_tools.sh"
cp "${REPO_ROOT}/.clang-format-version" "${SCOPED_ROOT}/.clang-format-version"
printf 'int example;\n' > "${SCOPED_ROOT}/src/selected file.cc"
printf 'int other;\n' > "${SCOPED_ROOT}/src/other.cc"
printf 'extern int example;\n' > "${SCOPED_ROOT}/src/selected file.h"

write_database() {
  python3 - "$SCOPED_BUILD" "$SCOPED_ROOT" "$@" <<'PY'
import json
import sys
from pathlib import Path

build, root, *files = sys.argv[1:]
entries = [{"directory": root, "file": name,
            "arguments": ["clang++", "-c", name]} for name in files]
(Path(build) / "compile_commands.json").write_text(json.dumps(entries))
PY
}

CLEAN_TIDY="$(make_stub clang-tidy-clean 0)"
WARNING_TIDY="$(make_stub clang-tidy-warning 0 'warning: advisory finding [readability-example]')"
FAILED_TIDY="$(make_stub clang-tidy-failed 1 'error: incompatible PCH [clang-diagnostic-error]')"
SCOPED_LINT="${SCOPED_ROOT}/scripts/lint.sh"

run_scoped_lint() {
  YAZE_CLANG_FORMAT="$CLEAN_FORMAT" YAZE_CLANG_TIDY="$CLEAN_TIDY" \
    bash "$SCOPED_LINT" check --build-dir "$SCOPED_BUILD" "$@"
}

write_database 'src/selected file.cc' 'src/other.cc'
rm -f "${WORK_DIR}/clang-format-clean.args" "${WORK_DIR}/clang-tidy-clean.args"
status=0
scoped_output="$(run_scoped_lint --require-tidy 'src/selected file.cc' 2>&1)" || status=$?
[[ "$status" -eq 0 ]] || fail "required tidy on covered TU returned ${status}: ${scoped_output}"
grep -qx 'src/selected file.cc' "${WORK_DIR}/clang-format-clean.args" ||
  fail "format split the selected filename containing spaces"
grep -qx 'src/selected file.cc' "${WORK_DIR}/clang-tidy-clean.args" ||
  fail "tidy split the selected filename containing spaces"
grep -qx "$SCOPED_BUILD" "${WORK_DIR}/clang-tidy-clean.args" ||
  fail "tidy did not receive the exact build directory"
if grep -q 'other.cc\|warnings-as-errors\|--fix' "${WORK_DIR}/clang-tidy-clean.args"; then
  fail "required mode widened source scope, promoted warnings, or enabled fixes"
fi

status=0
YAZE_CLANG_FORMAT="$CLEAN_FORMAT" YAZE_CLANG_TIDY="$WARNING_TIDY" \
  bash "$SCOPED_LINT" check --build-dir "$SCOPED_BUILD" --require-tidy \
  'src/selected file.cc' >/dev/null 2>&1 || status=$?
[[ "$status" -eq 0 ]] || fail "required mode made advisory warnings fail"

run_scoped_lint --require-tidy --warnings-as-errors 'clang-analyzer-*' \
  'src/selected file.cc' >/dev/null 2>&1 || fail "warnings-as-errors option failed"
grep -qx -- '--warnings-as-errors=clang-analyzer-\*' "${WORK_DIR}/clang-tidy-clean.args" ||
  fail "warnings-as-errors glob was not forwarded literally"

status=0
promotion_output="$(YAZE_CLANG_FORMAT="$CLEAN_FORMAT" YAZE_CLANG_TIDY="" \
  bash "$SCOPED_LINT" check --warnings-as-errors '*' 'src/selected file.cc' 2>&1)" || status=$?
[[ "$status" -eq 2 && "$promotion_output" == *"requires --require-tidy"* ]] ||
  fail "warning promotion without required analysis did not fail with usage guidance"

status=0
YAZE_CLANG_FORMAT="$CLEAN_FORMAT" YAZE_CLANG_TIDY="$FAILED_TIDY" \
  bash "$SCOPED_LINT" check --build-dir "$SCOPED_BUILD" --require-tidy \
  'src/selected file.cc' >/dev/null 2>&1 || status=$?
[[ "$status" -eq 1 ]] || fail "tidy parse failure returned ${status}, expected 1"

status=0
YAZE_CLANG_FORMAT="$CLEAN_FORMAT" YAZE_CLANG_TIDY="" \
  bash "$SCOPED_LINT" check --build-dir "$SCOPED_BUILD" --require-tidy \
  'src/selected file.cc' >/dev/null 2>&1 || status=$?
[[ "$status" -eq 3 ]] || fail "required mode without tidy returned ${status}, expected 3"

status=0
run_scoped_lint --require-tidy >/dev/null 2>&1 || status=$?
[[ "$status" -eq 2 ]] || fail "required mode without explicit scope returned ${status}, expected 2"

status=0
header_output="$(run_scoped_lint --require-tidy 'src/selected file.h' 2>&1)" || status=$?
[[ "$status" -eq 2 && "$header_output" == *"owning .cc files"* ]] ||
  fail "required header input did not require an owning TU"

write_database 'src/other.cc'
rm -f "${WORK_DIR}/clang-tidy-clean.args"
status=0
uncovered_output="$(run_scoped_lint --require-tidy 'src/selected file.cc' 2>&1)" || status=$?
[[ "$status" -eq 3 && "$uncovered_output" == *"no exact compile command"* ]] ||
  fail "uncovered TU did not fail required mode"
[[ ! -e "${WORK_DIR}/clang-tidy-clean.args" ]] ||
  fail "tidy ran despite missing compile entry"

# A borrowed worktree DB with the same relative filename does not cover ours.
write_database "${WORK_DIR}/other worktree/src/selected file.cc"
status=0
run_scoped_lint --require-tidy 'src/selected file.cc' >/dev/null 2>&1 || status=$?
[[ "$status" -eq 3 ]] || fail "foreign worktree compile command was accepted"

printf 'not json\n' > "${SCOPED_BUILD}/compile_commands.json"
status=0
run_scoped_lint --require-tidy 'src/selected file.cc' >/dev/null 2>&1 || status=$?
[[ "$status" -eq 3 ]] || fail "invalid compile database returned ${status}, expected 3"

printf '[null]\n' > "${SCOPED_BUILD}/compile_commands.json"
status=0
malformed_output="$(run_scoped_lint --require-tidy 'src/selected file.cc' 2>&1)" || status=$?
[[ "$status" -eq 3 && "$malformed_output" == *"unusable compile database"* && "$malformed_output" != *"Traceback"* ]] ||
  fail "nonobject compile entry did not produce a clean prerequisite failure"

rm -f "${SCOPED_BUILD}/compile_commands.json"
status=0
run_scoped_lint --require-tidy 'src/selected file.cc' >/dev/null 2>&1 || status=$?
[[ "$status" -eq 3 ]] || fail "missing explicit compile database returned ${status}, expected 3"

status=0
YAZE_CLANG_FORMAT="$CLEAN_FORMAT" YAZE_CLANG_TIDY="$CLEAN_TIDY" \
  bash "$SCOPED_LINT" check --require-tidy 'src/selected file.cc' >/dev/null 2>&1 || status=$?
[[ "$status" -eq 3 ]] || fail "missing default compile database returned ${status}, expected 3"

advisory_output="$(YAZE_CLANG_FORMAT="$CLEAN_FORMAT" YAZE_CLANG_TIDY="$CLEAN_TIDY" \
  bash "$SCOPED_LINT" check 'src/selected file.cc' 2>&1)" ||
  fail "legacy formatting-only mode failed"
[[ "$advisory_output" == *"no static-analysis coverage"* ]] ||
  fail "skipped tidy did not clearly report the coverage limitation"

if [[ "$failures" -ne 0 ]]; then
  echo "${failures} quality tooling contract check(s) failed" >&2
  exit 1
fi

echo "quality tooling contract tests passed"
