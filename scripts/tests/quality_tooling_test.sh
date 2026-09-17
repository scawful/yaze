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

if [[ "$failures" -ne 0 ]]; then
  echo "${failures} quality tooling contract check(s) failed" >&2
  exit 1
fi

echo "quality tooling contract tests passed"
