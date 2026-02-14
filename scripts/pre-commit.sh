#!/usr/bin/env bash
# Pre-commit validation script for yaze.
#
# Usage:
#   scripts/pre-commit.sh [--all] [--skip-format] [--skip-shell] [--skip-python] [--skip-version]
#
# Notes:
#   - Default mode checks staged files only.
#   - Set YAZE_SKIP_PRECOMMIT=1 to bypass in emergencies.

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_header() { echo -e "\n${BLUE}=== $1 ===${NC}"; }
print_ok() { echo -e "${GREEN}[ok] $1${NC}"; }
print_warn() { echo -e "${YELLOW}[warn] $1${NC}"; }
print_err() { echo -e "${RED}[err] $1${NC}"; }

if [[ "${YAZE_SKIP_PRECOMMIT:-0}" == "1" ]]; then
  print_warn "YAZE_SKIP_PRECOMMIT=1 set; skipping pre-commit checks"
  exit 0
fi

CHECK_ALL=false
SKIP_FORMAT=false
SKIP_SHELL=false
SKIP_PYTHON=false
SKIP_VERSION=false

while [[ $# -gt 0 ]]; do
  case "$1" in
    --all)
      CHECK_ALL=true
      shift
      ;;
    --skip-format)
      SKIP_FORMAT=true
      shift
      ;;
    --skip-shell)
      SKIP_SHELL=true
      shift
      ;;
    --skip-python)
      SKIP_PYTHON=true
      shift
      ;;
    --skip-version)
      SKIP_VERSION=true
      shift
      ;;
    --help|-h)
      sed -n '1,24p' "$0" | sed 's/^# \{0,1\}//'
      exit 0
      ;;
    *)
      print_err "Unknown option: $1"
      exit 2
      ;;
  esac
done

REPO_ROOT="$(git rev-parse --show-toplevel 2>/dev/null || true)"
if [[ -z "$REPO_ROOT" ]]; then
  print_err "Not in a git repository"
  exit 2
fi
cd "$REPO_ROOT"

if [[ "$CHECK_ALL" == true ]]; then
  mapfile -t CANDIDATES < <(git ls-files)
else
  mapfile -t CANDIDATES < <(git diff --cached --name-only --diff-filter=ACMR)
fi

if [[ ${#CANDIDATES[@]} -eq 0 ]]; then
  print_ok "No files to check"
  exit 0
fi

# Keep only files that still exist.
FILES=()
for f in "${CANDIDATES[@]}"; do
  [[ -f "$f" ]] && FILES+=("$f")
done

if [[ ${#FILES[@]} -eq 0 ]]; then
  print_ok "No existing files to check"
  exit 0
fi

CPP_FILES=()
SH_FILES=()
PY_FILES=()

for f in "${FILES[@]}"; do
  case "$f" in
    src/lib/*)
      continue
      ;;
  esac
  case "$f" in
    *.cc|*.cpp|*.cxx|*.h|*.hpp|*.hh)
      CPP_FILES+=("$f")
      ;;
    *.sh)
      SH_FILES+=("$f")
      ;;
    *.py)
      PY_FILES+=("$f")
      ;;
    *)
      # Also validate extensionless shell entrypoints like scripts/yaze and scripts/z3ed.
      if [[ "$f" == scripts/* ]]; then
        if head -n 1 "$f" | grep -Eq '^#!.*(ba|z)?sh'; then
          SH_FILES+=("$f")
        fi
      fi
      ;;
  esac
done

find_clang_format() {
  local names=(clang-format-18 clang-format-17 clang-format)
  local n
  for n in "${names[@]}"; do
    if command -v "$n" >/dev/null 2>&1; then
      echo "$n"
      return 0
    fi
  done
  if [[ "$(uname -s)" == "Darwin" ]] && command -v brew >/dev/null 2>&1; then
    local p
    p="$(brew --prefix llvm 2>/dev/null || true)"
    if [[ -n "$p" && -x "$p/bin/clang-format" ]]; then
      echo "$p/bin/clang-format"
      return 0
    fi
  fi
  return 1
}

if [[ "$SKIP_FORMAT" == false && ${#CPP_FILES[@]} -gt 0 ]]; then
  print_header "clang-format"
  CLANG_FORMAT="$(find_clang_format || true)"
  if [[ -z "$CLANG_FORMAT" ]]; then
    print_err "clang-format not found (required for C/C++ staged files)"
    exit 3
  fi
  "$CLANG_FORMAT" --dry-run --Werror --style=file "${CPP_FILES[@]}"
  print_ok "C/C++ formatting check passed (${#CPP_FILES[@]} files)"
fi

if [[ "$SKIP_SHELL" == false && ${#SH_FILES[@]} -gt 0 ]]; then
  print_header "shell syntax"
  local_fail=0
  for f in "${SH_FILES[@]}"; do
    if ! bash -n "$f"; then
      print_err "bash -n failed: $f"
      local_fail=1
    fi
  done
  if [[ $local_fail -ne 0 ]]; then
    exit 4
  fi
  print_ok "Shell syntax check passed (${#SH_FILES[@]} files)"
fi

if [[ "$SKIP_PYTHON" == false && ${#PY_FILES[@]} -gt 0 ]]; then
  print_header "python syntax"
  if ! command -v python3 >/dev/null 2>&1; then
    print_err "python3 not found (required for Python staged files)"
    exit 5
  fi
  python3 -m py_compile "${PY_FILES[@]}"
  print_ok "Python syntax check passed (${#PY_FILES[@]} files)"
fi

if [[ "$SKIP_VERSION" == false ]]; then
  print_header "release/versioning"
  scripts/dev/release-version-check.sh --staged
fi

print_header "summary"
print_ok "pre-commit checks passed"
