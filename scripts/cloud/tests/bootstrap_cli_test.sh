#!/usr/bin/env bash
# Hermetic tests for scripts/cloud/bootstrap.sh step dispatch: default and
# `all` expansion order, unknown steps, and --help. Step bodies are stubbed,
# so nothing is installed, cloned, or built.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BOOTSTRAP="$SCRIPT_DIR/../bootstrap.sh"
# Resolved from this script's absolute directory.
# shellcheck disable=SC1090,SC1091
source "$BOOTSTRAP"

fail() {
  echo "bootstrap cli test failed: $*" >&2
  exit 1
}

CALLS=()
for step in deps submodules refs configure build test; do
  eval "step_${step}() { CALLS+=(${step}); }"
done
log() { :; }

run_main() {
  CALLS=()
  main "$@"
}

run_main
[[ "${CALLS[*]}" == "deps submodules refs" ]] ||
  fail "default steps ran '${CALLS[*]}'"

run_main all
[[ "${CALLS[*]}" == "deps submodules refs configure build test" ]] ||
  fail "all ran '${CALLS[*]}'"

run_main configure build
[[ "${CALLS[*]}" == "configure build" ]] ||
  fail "explicit steps ran '${CALLS[*]}'"

status=0
run_main refs bogus build 2>/dev/null || status=$?
[[ "$status" -eq 2 ]] || fail "unknown step returned $status, expected 2"
[[ "${CALLS[*]}" == "refs" ]] ||
  fail "steps after an unknown step ran: '${CALLS[*]}'"

# --help runs as a real subprocess so the header awk sees the file itself.
help_output="$(bash "$BOOTSTRAP" --help)"
[[ "$help_output" == *"Usage: scripts/cloud/bootstrap.sh [steps...]"* ]] ||
  fail "--help is missing the usage line"
[[ "$help_output" == *"YAZE_BUILD_JOBS"* ]] ||
  fail "--help stops before the environment section"
[[ "$help_output" != *"set -euo pipefail"* ]] ||
  fail "--help printed script code"

unknown_stderr="$(bash "$BOOTSTRAP" nope 2>&1 >/dev/null || true)"
[[ "$unknown_stderr" == *"unknown step: nope"* ]] ||
  fail "unknown step message missing: '$unknown_stderr'"

echo "bootstrap cli tests passed"
