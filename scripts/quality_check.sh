#!/usr/bin/env bash
# Whole-repository code quality pass for yaze.
#
# Usage:
#   scripts/quality_check.sh [--advisory | --gate] [--help]
#
#   --advisory  Report findings and exit 0. This is the default.
#   --gate      Exit non-zero when there are actionable findings.
#
# Actionable findings are clang-format violations under src/ and test/ and
# cppcheck error-severity findings under src/. cppcheck warning, style, and
# performance output stays advisory in both modes, as do the syntaxError and
# unknownMacro findings that come from cppcheck failing to parse our headers.
#
# This is the slow whole-repository pass. scripts/lint.sh is the changed-file
# fast path and is the entry point that runs clang-tidy; running clang-tidy
# across every translation unit here would take longer than a build.
#
# Exit codes:
#   0  no actionable findings, or advisory mode
#   1  actionable findings (gate mode)
#   2  usage error
#   3  a required tool is missing (gate mode)

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
# shellcheck source=scripts/lib/clang_tools.sh
source "${SCRIPT_DIR}/lib/clang_tools.sh"

usage() {
  awk 'NR == 1 { next } /^#/ { sub(/^# ?/, ""); print; next } { exit }' "$0"
}

MODE="advisory"
while [[ $# -gt 0 ]]; do
  case "$1" in
    --advisory)
      MODE="advisory"
      shift
      ;;
    --gate)
      MODE="gate"
      shift
      ;;
    --help | -h)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

cd "${PROJECT_ROOT}" || exit 3

WORK_DIR="$(mktemp -d)"
trap 'rm -rf "${WORK_DIR}"' EXIT

ACTIONABLE=()
MISSING_TOOLS=()

note_actionable() { ACTIONABLE+=("$1"); }

echo "Quality checks for yaze (${MODE} mode)"

# --- Formatting -------------------------------------------------------------

echo
echo "== clang-format =="

if [[ ! -f .clang-format ]]; then
  echo "Missing .clang-format at the repository root." >&2
  echo "Restore it from git rather than regenerating: the checked-in file is" \
    "the style contract shared with CI." >&2
  exit 3
fi

CLANG_FORMAT="$(yaze_find_clang_format || true)"
if [[ -z "${CLANG_FORMAT}" ]]; then
  echo "clang-format not found; skipping the formatting check."
  MISSING_TOOLS+=("clang-format")
else
  yaze_warn_clang_version "${CLANG_FORMAT}" clang-format
  echo "Using ${CLANG_FORMAT}"

  SOURCE_FILES=()
  if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    mapfile -t SOURCE_FILES < <(
      git ls-files 'src/*.cc' 'src/*.h' 'test/*.cc' 'test/*.h' |
        grep -v '^src/lib/'
    )
  else
    mapfile -t SOURCE_FILES < <(
      find src test \( -name '*.cc' -o -name '*.h' \) -type f |
        grep -v '^src/lib/'
    )
  fi

  if [[ ${#SOURCE_FILES[@]} -eq 0 ]]; then
    echo "No C/C++ sources found under src/ or test/."
  else
    FORMAT_LOG="${WORK_DIR}/clang-format.log"
    if printf '%s\n' "${SOURCE_FILES[@]}" |
      xargs "${CLANG_FORMAT}" --dry-run --Werror --style=file \
        >"${FORMAT_LOG}" 2>&1; then
      echo "${#SOURCE_FILES[@]} files are formatted correctly."
    else
      FORMAT_FILES="$(
        sed -n 's/^\([^:]*\):[0-9]*:[0-9]*: error:.*/\1/p' "${FORMAT_LOG}" |
          sort -u
      )"
      FORMAT_COUNT="$(printf '%s\n' "${FORMAT_FILES}" | grep -c .)"
      echo "Formatting violations in ${FORMAT_COUNT} file(s):"
      printf '%s\n' "${FORMAT_FILES}" | head -20
      echo "Fix with: scripts/lint.sh fix"
      note_actionable "clang-format: ${FORMAT_COUNT} file(s) need formatting"
    fi
  fi
fi

# --- Static analysis --------------------------------------------------------

echo
echo "== cppcheck =="

CPPCHECK="$(yaze_find_cppcheck || true)"
if [[ -z "${CPPCHECK}" ]]; then
  echo "cppcheck not found; skipping static analysis."
  MISSING_TOOLS+=("cppcheck")
else
  CPPCHECK_LOG="${WORK_DIR}/cppcheck.log"
  "${CPPCHECK}" \
    --enable=warning,style,performance \
    --inconclusive \
    --quiet \
    --suppress=missingIncludeSystem \
    --suppress=unusedFunction \
    --suppress=unmatchedSuppression \
    --suppress=unreadVariable \
    --suppress=cstyleCast \
    --suppress=variableScope \
    --template='{file}:{line}: {severity}: {message} [{id}]' \
    src/ >"${CPPCHECK_LOG}" 2>&1 || true

  # cppcheck cannot expand our ImGui/absl macros, so syntaxError and
  # unknownMacro say more about cppcheck than about the code.
  CPPCHECK_ERRORS="${WORK_DIR}/cppcheck-errors.log"
  grep ': error: ' "${CPPCHECK_LOG}" |
    grep -vE '\[(syntaxError|unknownMacro|internalAstError|preprocessorErrorDirective)\]$' \
      >"${CPPCHECK_ERRORS}" || true

  ADVISORY_COUNT="$(grep -cE ': (warning|style|performance|portability): ' "${CPPCHECK_LOG}")"
  ERROR_COUNT="$(grep -c . "${CPPCHECK_ERRORS}")"

  echo "${ADVISORY_COUNT} advisory finding(s) (warning/style/performance/portability):"
  grep -E ': (warning|style|performance|portability): ' "${CPPCHECK_LOG}" | head -10

  if [[ "${ERROR_COUNT}" -gt 0 ]]; then
    echo
    echo "${ERROR_COUNT} error-severity finding(s):"
    cat "${CPPCHECK_ERRORS}"
    note_actionable "cppcheck: ${ERROR_COUNT} error-severity finding(s)"
  else
    echo
    echo "No error-severity findings."
  fi
fi

# --- Summary ----------------------------------------------------------------

echo
echo "== summary =="

if [[ ${#MISSING_TOOLS[@]} -gt 0 ]]; then
  echo "Missing tools: ${MISSING_TOOLS[*]}"
  if [[ "${MODE}" == "gate" ]]; then
    echo "Gate mode needs every tool present to make a pass meaningful." >&2
    exit 3
  fi
fi

if [[ ${#ACTIONABLE[@]} -eq 0 ]]; then
  echo "No actionable findings."
  exit 0
fi

echo "Actionable findings:"
printf '  - %s\n' "${ACTIONABLE[@]}"

if [[ "${MODE}" == "gate" ]]; then
  exit 1
fi

echo "Advisory mode: exiting 0. Re-run with --gate to fail on these."
exit 0
