#!/usr/bin/env bash
# Pre-push validation script for yaze.
# Fast-by-default checks with change-aware UI regression coverage.
#
# Usage:
#   scripts/pre-push.sh [--skip-tests] [--skip-format] [--skip-build] [--skip-ui-regression] [--build-dir <dir>]
#
# Options:
#   --skip-tests          Skip unit smoke tests and UI regression tests
#   --skip-format         Skip format-check target
#   --skip-build          Skip build verification
#   --skip-ui-regression  Skip change-aware UI regression tests
#   --build-dir <dir>     Build directory to use (default: build)
#   --help                Show this help text
#
# Env:
#   YAZE_PREPUSH_BUILD_DIR   Build directory override
#   YAZE_PREPUSH_TIMEOUT     Test timeout in seconds (default: 120)
#   YAZE_PREPUSH_SMOKE_FILTER
#   YAZE_PREPUSH_UI_FILTER

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

SKIP_TESTS=false
SKIP_FORMAT=false
SKIP_BUILD=false
SKIP_UI_REGRESSION=false
BUILD_DIR="${YAZE_PREPUSH_BUILD_DIR:-build}"
TEST_TIMEOUT="${YAZE_PREPUSH_TIMEOUT:-120}"
SMOKE_TEST_FILTER="${YAZE_PREPUSH_SMOKE_FILTER:-PanelManagerPolicyTest.*}"
UI_TEST_FILTER="${YAZE_PREPUSH_UI_FILTER:-PanelManagerPolicyTest.*:UserSettingsLayoutDefaultsTest.*:LayoutPresetsTest.*:AnimatorTest.*}"

print_header() { echo -e "\n${BLUE}=== $1 ===${NC}"; }
print_ok() { echo -e "${GREEN}[ok] $1${NC}"; }
print_err() { echo -e "${RED}[err] $1${NC}"; }
print_warn() { echo -e "${YELLOW}[warn] $1${NC}"; }
print_info() { echo -e "${BLUE}[info] $1${NC}"; }

while [[ $# -gt 0 ]]; do
  case "$1" in
    --skip-tests)
      SKIP_TESTS=true
      shift
      ;;
    --skip-format)
      SKIP_FORMAT=true
      shift
      ;;
    --skip-build)
      SKIP_BUILD=true
      shift
      ;;
    --skip-ui-regression)
      SKIP_UI_REGRESSION=true
      shift
      ;;
    --build-dir)
      BUILD_DIR="$2"
      shift 2
      ;;
    --help|-h)
      sed -n '1,28p' "$0" | sed 's/^# \{0,1\}//'
      exit 0
      ;;
    *)
      print_err "Unknown option: $1"
      exit 4
      ;;
  esac
done

check_git_repo() {
  if ! git rev-parse --git-dir >/dev/null 2>&1; then
    print_err "Not in a git repository"
    exit 4
  fi
}

check_cmake_configured() {
  if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    print_warn "Build directory not configured: $BUILD_DIR"
    print_info "Configure first: cmake --preset dev"
    return 1
  fi
  return 0
}

run_with_timeout() {
  local timeout_s="$1"
  shift
  if command -v timeout >/dev/null 2>&1; then
    timeout "$timeout_s" "$@"
    return $?
  fi
  if command -v gtimeout >/dev/null 2>&1; then
    gtimeout "$timeout_s" "$@"
    return $?
  fi

  print_warn "timeout utility not found; running without timeout"
  "$@"
}

resolve_test_binary() {
  local name="$1"
  local candidates=(
    "$BUILD_DIR/bin/Debug/$name"
    "$BUILD_DIR/bin/RelWithDebInfo/$name"
    "$BUILD_DIR/bin/Release/$name"
    "$BUILD_DIR/bin/$name"
    "build_ai/bin/Debug/$name"
    "build_ai/bin/RelWithDebInfo/$name"
    "build_ai/bin/Release/$name"
    "build_ai/bin/$name"
  )

  local c
  for c in "${candidates[@]}"; do
    if [[ -x "$c" ]]; then
      echo "$c"
      return 0
    fi
  done
  return 1
}

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
    local llvm_prefix
    llvm_prefix="$(brew --prefix llvm 2>/dev/null || true)"
    if [[ -n "$llvm_prefix" && -x "$llvm_prefix/bin/clang-format" ]]; then
      echo "$llvm_prefix/bin/clang-format"
      return 0
    fi
  fi

  return 1
}

collect_changed_files() {
  local base=""
  local committed=()
  local staged=()
  local working=()

  if git rev-parse --verify '@{upstream}' >/dev/null 2>&1; then
    base="$(git merge-base HEAD '@{upstream}' 2>/dev/null || true)"
  fi

  if [[ -n "$base" ]]; then
    mapfile -t committed < <(git diff --name-only --diff-filter=ACMR "$base"...HEAD)
  elif git rev-parse --verify HEAD~1 >/dev/null 2>&1; then
    mapfile -t committed < <(git diff --name-only --diff-filter=ACMR HEAD~1...HEAD)
  fi

  mapfile -t staged < <(git diff --cached --name-only --diff-filter=ACMR)
  mapfile -t working < <(git diff --name-only --diff-filter=ACMR)

  printf '%s\n' "${committed[@]}" "${staged[@]}" "${working[@]}" | sed '/^$/d' | sort -u
}

has_ui_workflow_changes() {
  local files=("$@")
  local f
  for f in "${files[@]}"; do
    case "$f" in
      src/app/editor/menu/activity_bar.cc|src/app/editor/menu/activity_bar.h|\
      src/app/editor/menu/right_panel_manager.cc|src/app/editor/menu/right_panel_manager.h|\
      src/app/editor/system/panel_manager.cc|src/app/editor/system/panel_manager.h|\
      src/app/editor/dungeon/dungeon_editor_v2.cc|src/app/editor/dungeon/dungeon_editor_v2.h|\
      src/app/editor/dungeon/panels/dungeon_workbench_panel.cc|src/app/editor/dungeon/panels/dungeon_workbench_panel.h|\
      src/app/gui/widgets/themed_widgets.cc|src/app/gui/widgets/themed_widgets.h|\
      src/app/gui/core/ui_config.h|\
      src/app/gui/animation/animator.cc|src/app/gui/animation/animator.h|\
      test/unit/editor/panel_manager_policy_test.cc|\
      test/unit/editor/user_settings_layout_defaults_test.cc|\
      test/unit/editor/layout_presets_test.cc|\
      test/unit/gui/animator_test.cc)
        return 0
        ;;
    esac
  done
  return 1
}

main() {
  print_header "Pre-Push Validation"
  check_git_repo

  if ! check_cmake_configured; then
    exit 4
  fi

  local start_time
  start_time="$(date +%s)"

  mapfile -t CHANGED_FILES < <(collect_changed_files | sed '/^$/d' | sort -u)
  print_info "Changed files considered: ${#CHANGED_FILES[@]}"

  if [[ "$SKIP_BUILD" == false ]]; then
    print_header "Step 1/4: Build Verification"
    local jobs="${YAZE_BUILD_JOBS:-8}"
    print_info "Building yaze_test_unit (jobs=$jobs)"
    if ! cmake --build "$BUILD_DIR" --target yaze_test_unit --parallel "$jobs"; then
      print_err "Build failed"
      exit 1
    fi
    print_ok "Build passed"
  else
    print_warn "Skipping build verification (--skip-build)"
  fi

  if [[ "$SKIP_TESTS" == false ]]; then
    print_header "Step 2/4: Unit Smoke Tests"

    local smoke_bin=""
    smoke_bin="$(resolve_test_binary yaze_test_unit || true)"
    if [[ -z "$smoke_bin" ]]; then
      smoke_bin="$(resolve_test_binary yaze_test || true)"
    fi
    if [[ -z "$smoke_bin" ]]; then
      print_err "Could not locate yaze test binary"
      print_info "Expected e.g. $BUILD_DIR/bin/Debug/yaze_test_unit"
      exit 2
    fi

    print_info "Running smoke filter: $SMOKE_TEST_FILTER"
    if ! run_with_timeout "$TEST_TIMEOUT" "$smoke_bin" \
        --gtest_filter="$SMOKE_TEST_FILTER" --gtest_brief=1; then
      print_err "Unit smoke tests failed"
      exit 2
    fi
    print_ok "Unit smoke tests passed"
  else
    print_warn "Skipping unit smoke tests (--skip-tests)"
  fi

  if [[ "$SKIP_FORMAT" == false ]]; then
    print_header "Step 3/4: Code Formatting"

    local cpp_changed=()
    local f
    for f in "${CHANGED_FILES[@]}"; do
      case "$f" in
        *.cc|*.cpp|*.cxx|*.h|*.hpp|*.hh)
          if [[ -f "$f" ]]; then
            cpp_changed+=("$f")
          fi
          ;;
      esac
    done

    if [[ ${#cpp_changed[@]} -eq 0 ]]; then
      print_info "No changed C/C++ files detected; skipping format check"
    else
      local clang_fmt
      clang_fmt="$(find_clang_format || true)"
      if [[ -z "$clang_fmt" ]]; then
        print_err "clang-format not found"
        exit 3
      fi

      print_info "Checking formatting for ${#cpp_changed[@]} changed C/C++ files"
      if ! "$clang_fmt" --dry-run --Werror --style=file "${cpp_changed[@]}"; then
        print_err "Code formatting check failed for changed files"
        print_info "Fix with: scripts/lint.sh fix"
        exit 3
      fi
      print_ok "Code formatting passed"
    fi
  else
    print_warn "Skipping format check (--skip-format)"
  fi

  if [[ "$SKIP_TESTS" == true || "$SKIP_UI_REGRESSION" == true ]]; then
    if [[ "$SKIP_UI_REGRESSION" == true ]]; then
      print_warn "Skipping UI regression tests (--skip-ui-regression)"
    fi
  else
    print_header "Step 4/4: UI Regression (Change-Aware)"
    if has_ui_workflow_changes "${CHANGED_FILES[@]}"; then
      local ui_bin=""
      ui_bin="$(resolve_test_binary yaze_test_unit || true)"
      if [[ -z "$ui_bin" ]]; then
        ui_bin="$(resolve_test_binary yaze_test || true)"
      fi
      if [[ -z "$ui_bin" ]]; then
        print_err "Could not locate yaze test binary for UI regression"
        exit 2
      fi

      print_info "Detected panel/workflow-related changes"
      print_info "Running UI filter: $UI_TEST_FILTER"
      if ! run_with_timeout "$TEST_TIMEOUT" "$ui_bin" \
          --gtest_filter="$UI_TEST_FILTER" --gtest_brief=1; then
        print_err "UI regression tests failed"
        exit 2
      fi
      print_ok "UI regression tests passed"
    else
      print_info "No panel/workflow files changed; skipping UI regression step"
    fi
  fi

  local end_time
  end_time="$(date +%s)"
  local duration=$((end_time - start_time))

  print_header "Pre-Push Validation Complete"
  print_ok "All checks passed in ${duration}s"
}

main "$@"
