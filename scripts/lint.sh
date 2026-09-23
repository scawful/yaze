#!/usr/bin/env bash
# Scoped formatting and static analysis for yaze.
#
# Usage:
#   scripts/lint.sh [check|fix] [options] [files...]
#
#   check (default)                 Check without modifying files.
#   fix                             Apply formatting and clang-tidy fixes.
#   --build-dir <directory>         Use its compile_commands.json explicitly.
#   --require-tidy                  Require named translation units, a matching
#                                   compile database, and successful analysis.
#   --warnings-as-errors <glob>     Promote matching tidy warnings to errors.
#                                   Requires --require-tidy for a nonempty glob.
#   --help                          Print this help.
#
# File paths and build directories are relative to the repository root unless
# absolute. With no files, legacy advisory mode checks all src/ and test/ files;
# --require-tidy requires explicit .cc/.cpp/.cxx/.c/.mm files. Format headers
# separately and analyze their owning translation units rather than guessing
# compiler flags for a header. Warnings remain advisory unless explicitly gated.
#
# Without --build-dir, use the repo-root compile_commands.json symlink maintained
# by scripts/dev/update_compile_commands.sh, then the legacy build/ fallback.
# A default run may skip tidy; its output does not establish analysis coverage.
# --require-tidy fails instead of skipping missing tools, databases, or entries.
#
# Exit codes: 0 completed checks; 1 findings/tool failure; 2 usage error;
#             3 required analysis prerequisites unavailable.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
# shellcheck source=scripts/lib/clang_tools.sh
source "${SCRIPT_DIR}/lib/clang_tools.sh"
cd "$PROJECT_ROOT"

usage() {
  awk 'NR == 1 { next } /^#/ { sub(/^# ?/, ""); print; next } { exit }' "$0"
}

usage_error() {
  echo "Error: $*" >&2
  echo "Run scripts/lint.sh --help for usage." >&2
  exit 2
}

MODE="check"
BUILD_PATH=""
REQUIRE_TIDY=0
WARNINGS_AS_ERRORS=""
FILES=()

if [[ "${1:-}" == "check" || "${1:-}" == "fix" ]]; then
  MODE="$1"
  shift
fi

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)
      [[ $# -ge 2 && -n "$2" ]] || usage_error "--build-dir needs a directory"
      BUILD_PATH="$2"
      shift 2
      ;;
    --require-tidy)
      REQUIRE_TIDY=1
      shift
      ;;
    --warnings-as-errors)
      [[ $# -ge 2 ]] || usage_error "--warnings-as-errors needs a quoted glob"
      WARNINGS_AS_ERRORS="$2"
      shift 2
      ;;
    --warnings-as-errors=*)
      WARNINGS_AS_ERRORS="${1#*=}"
      shift
      ;;
    --help | -h)
      usage
      exit 0
      ;;
    --)
      shift
      FILES+=("$@")
      break
      ;;
    -*) usage_error "unknown option: $1" ;;
    *)
      FILES+=("$1")
      shift
      ;;
  esac
done

if [[ -n "$WARNINGS_AS_ERRORS" && "$REQUIRE_TIDY" -eq 0 ]]; then
  usage_error "--warnings-as-errors requires --require-tidy so an unavailable analysis cannot pass"
fi

if [[ ${#FILES[@]} -eq 0 ]]; then
  [[ "$REQUIRE_TIDY" -eq 0 ]] || usage_error "--require-tidy needs explicit translation-unit files"
  while IFS= read -r -d '' source_file; do
    [[ "$source_file" == src/lib/* ]] || FILES+=("$source_file")
  done < <(
    if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
      git ls-files -z 'src/*.cc' 'src/*.h' 'test/*.cc' 'test/*.h'
    else
      find src test -type f \( -name '*.cc' -o -name '*.h' \) -print0
    fi
  )
fi

[[ ${#FILES[@]} -gt 0 ]] || usage_error "no source files selected"
for source_file in "${FILES[@]}"; do
  [[ -f "$source_file" ]] || usage_error "source file not found: $source_file"
  if [[ "$REQUIRE_TIDY" -eq 1 ]]; then
    case "$source_file" in
      *.cc | *.cpp | *.cxx | *.c | *.mm) ;;
      *) usage_error "--require-tidy needs a translation unit, not $source_file; format headers separately and pass their owning .cc files" ;;
    esac
  fi
done

CLANG_FORMAT="$(yaze_find_clang_format || true)"
CLANG_TIDY="$(yaze_find_clang_tidy || true)"
if [[ -z "$CLANG_FORMAT" ]]; then
  echo "Error: clang-format not found." >&2
  echo "Install major $(yaze_clang_pinned_major) to match CI (.clang-format-version)." >&2
  exit 1
fi

if [[ -n "$BUILD_PATH" ]]; then
  if [[ ! -f "$BUILD_PATH/compile_commands.json" ]]; then
    echo "Error: no compile_commands.json in requested build directory: $BUILD_PATH" >&2
    exit 3
  fi
elif [[ -f compile_commands.json ]]; then
  BUILD_PATH="."
elif [[ -f build/compile_commands.json ]]; then
  BUILD_PATH="build"
fi

if [[ "$REQUIRE_TIDY" -eq 1 ]]; then
  if [[ -z "$CLANG_TIDY" ]] || ! command -v "$CLANG_TIDY" >/dev/null 2>&1; then
    echo "Error: --require-tidy requested but clang-tidy not found." >&2
    exit 3
  fi
  if [[ -z "$BUILD_PATH" ]]; then
    echo "Error: --require-tidy needs compile_commands.json; pass --build-dir <configured-build>." >&2
    exit 3
  fi
  if ! command -v python3 >/dev/null 2>&1; then
    echo "Error: python3 is needed to verify compile database coverage." >&2
    exit 3
  fi
  # clang-tidy may infer a command for a file absent from the database. Required
  # mode must establish exact coverage, including when a worktree DB is stale.
  if ! python3 - "$BUILD_PATH/compile_commands.json" "${FILES[@]}" <<'PY'
import json
import sys
from pathlib import Path

database = Path(sys.argv[1]).resolve()
try:
    entries = json.loads(database.read_text())
    if not isinstance(entries, list):
        raise ValueError("expected an array of compile commands")
    covered = set()
    for entry in entries:
        directory = Path(entry["directory"])
        if not directory.is_absolute():
            directory = database.parent / directory
        if not entry.get("command") and not entry.get("arguments"):
            raise ValueError("compile command entry has no command or arguments")
        covered.add((directory / entry["file"]).resolve())
    missing = [name for name in sys.argv[2:] if Path(name).resolve() not in covered]
    if missing:
        for name in missing:
            print(f"Error: no exact compile command for {name}", file=sys.stderr)
        print("Configure this worktree's build with the selected sources enabled.", file=sys.stderr)
        sys.exit(1)
except (OSError, ValueError, TypeError, KeyError) as error:
    print(f"Error: unusable compile database {database}: {error}", file=sys.stderr)
    sys.exit(1)
PY
  then
    exit 3
  fi
fi

yaze_warn_clang_version "$CLANG_FORMAT" clang-format
echo "Using clang-format: $CLANG_FORMAT"
echo "Selected ${#FILES[@]} file(s)."
echo "=== Running clang-format ==="
FORMAT_ARGS=(--dry-run --Werror --style=file)
[[ "$MODE" != "fix" ]] || FORMAT_ARGS=(-i --style=file)
if ! printf '%s\0' "${FILES[@]}" | xargs -0 "$CLANG_FORMAT" "${FORMAT_ARGS[@]}"; then
  echo "Format check failed. Use scripts/lint.sh fix with the same file list to apply formatting." >&2
  exit 1
fi

if [[ -z "$CLANG_TIDY" || -z "$BUILD_PATH" ]]; then
  echo "Skipped clang-tidy: tool or compile_commands.json unavailable."
  echo "Linting complete: formatting only; no static-analysis coverage."
  exit 0
fi

echo "=== Running clang-tidy ==="
echo "Using clang-tidy: $CLANG_TIDY"
"$CLANG_TIDY" --version
echo "Compile database: $BUILD_PATH/compile_commands.json"
TIDY_ARGS=(-p "$BUILD_PATH" --quiet)
[[ "$MODE" != "fix" ]] || TIDY_ARGS+=(--fix)
[[ -z "$WARNINGS_AS_ERRORS" ]] || TIDY_ARGS+=("--warnings-as-errors=$WARNINGS_AS_ERRORS")
if ! "$CLANG_TIDY" "${TIDY_ARGS[@]}" "${FILES[@]}"; then
  echo "Clang-tidy failed: inspect diagnostics; this run does not establish a clean analysis." >&2
  exit 1
fi
echo "Linting complete: selected files analyzed; warnings are advisory unless promoted to errors."
