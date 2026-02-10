#!/usr/bin/env bash
# Fast local sanity checks for quick iteration.
#
# Default: run a small, high-signal subset of stable unit + integration tests
# via `ctest -R` (test-name regex).
#
# Full stable suite remains available via `--full` (ctest -L stable).

set -euo pipefail

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

DEFAULT_PRESET=""
case "${OSTYPE:-}" in
  darwin*) DEFAULT_PRESET="mac-ai" ;;
  linux*) DEFAULT_PRESET="lin-ai" ;;
  msys*|cygwin*|win32*) DEFAULT_PRESET="win-ai" ;;
  *) DEFAULT_PRESET="mac-ai" ;;
esac

PRESET="${YAZE_PRESET:-$DEFAULT_PRESET}"
BUILD_DIR="${YAZE_BUILD_DIR:-$ROOT_DIR/build_ai}"
CONFIG="${YAZE_CONFIG:-Debug}"
JOBS="${YAZE_JOBS:-8}"

UNIT_REGEX_DEFAULT="^(HackManifestTest|ResourceLabelsTest|OracleProgressionStateTest|AsarWrapperTest|AsarCompilerReproTest|WaterFillZoneTest)\\."
INTEGRATION_REGEX_DEFAULT="^DungeonSaveRegionTest\\."
UNIT_REGEX="${YAZE_FAST_UNIT_REGEX:-$UNIT_REGEX_DEFAULT}"
INTEGRATION_REGEX="${YAZE_FAST_INTEGRATION_REGEX:-$INTEGRATION_REGEX_DEFAULT}"

RUN_UNIT=1
RUN_INTEGRATION=1
DO_CONFIGURE=1
DO_BUILD=1
MODE="fast" # fast (regex subset), quick (labeled suite), or full (stable label)
LIST_ONLY=0
CTEST_REGEX=""

mktemp_file() {
  mktemp "${TMPDIR:-/tmp}/yaze-fasttest.XXXXXX"
}

run_quiet_or_die() {
  local label="$1"
  shift

  local tmp
  tmp="$(mktemp_file)"
  if "$@" >"$tmp" 2>&1; then
    rm -f "$tmp"
    return 0
  fi

  echo -e "${RED}✗${NC} ${label} failed. Output:" >&2
  cat "$tmp" >&2
  rm -f "$tmp"
  return 1
}

usage() {
  cat <<EOF
Usage: $0 [options]

Fast loop (default):
  - configures (cmake preset)
  - builds yaze_test_unit + yaze_test_integration
  - runs a curated subset via ctest name regex (-R)

Options:
  --quick                 Run the quick labeled suites (fastest; builds smaller targets).
  --full                  Run full stable suite via ctest (-L stable).
  --list                  List matching tests and exit (ctest -N).
  --unit-only              Only run the unit subset.
  --integration-only       Only run the integration subset.
  --no-configure           Skip cmake configure step.
  --no-build               Skip build step.
  --preset <name>          CMake preset (default: ${DEFAULT_PRESET}).
  --build-dir <path>       Build directory (default: build_ai).
  --config <cfg>           Multi-config build config (Debug/RelWithDebInfo/Release). Default: Debug.
  --jobs <n>               Parallel build/test jobs (default: 8).
  --filter <regex>         Extra ctest -R filter. In fast mode, overrides the
                           unit/integration regex subsets.
  --unit-regex <regex>     Unit test-name regex passed to ctest -R.
  --integration-regex <regex> Integration test-name regex passed to ctest -R.
  --unit-filter <regex>    Alias for --unit-regex.
  --integration-filter <regex> Alias for --integration-regex.
  -h, --help               Show this help.

Environment overrides:
  YAZE_PRESET, YAZE_BUILD_DIR, YAZE_CONFIG, YAZE_JOBS
  YAZE_FAST_UNIT_REGEX, YAZE_FAST_INTEGRATION_REGEX

Examples:
  $0
  $0 --quick
  $0 --quick --filter 'DungeonEditorV2RomSafetyTest'
  $0 --unit-only
  $0 --full
  $0 --unit-regex '^WaterFillZoneTest\\.'
EOF
}

is_multi_config() {
  local cache="$BUILD_DIR/CMakeCache.txt"
  [[ -f "$cache" ]] && grep -q "^CMAKE_CONFIGURATION_TYPES:" "$cache"
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --quick)
      MODE="quick"
      shift
      ;;
    --full)
      MODE="full"
      shift
      ;;
    --list)
      LIST_ONLY=1
      shift
      ;;
    --unit-only)
      RUN_INTEGRATION=0
      shift
      ;;
    --integration-only)
      RUN_UNIT=0
      shift
      ;;
    --no-configure)
      DO_CONFIGURE=0
      shift
      ;;
    --no-build)
      DO_BUILD=0
      shift
      ;;
    --preset)
      PRESET="$2"
      shift 2
      ;;
    --build-dir)
      BUILD_DIR="$2"
      shift 2
      ;;
    --config)
      CONFIG="$2"
      shift 2
      ;;
    --jobs)
      JOBS="$2"
      shift 2
      ;;
    --filter|--ctest-regex|--regex)
      CTEST_REGEX="$2"
      shift 2
      ;;
    --unit-regex|--unit-filter)
      UNIT_REGEX="$2"
      shift 2
      ;;
    --integration-regex|--integration-filter)
      INTEGRATION_REGEX="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo -e "${RED}Unknown option:${NC} $1" >&2
      echo "" >&2
      usage >&2
      exit 2
      ;;
  esac
done

cd "$ROOT_DIR"

echo -e "${BLUE}=== yaze fast tests ===${NC}"
echo "preset:    $PRESET"
echo "build dir: $BUILD_DIR"
echo "config:    $CONFIG"
echo "jobs:      $JOBS"
echo ""

if [[ -n "$CTEST_REGEX" ]]; then
  # For fast mode, keep behavior simple: the filter is the subset.
  UNIT_REGEX="$CTEST_REGEX"
  INTEGRATION_REGEX="$CTEST_REGEX"
fi

if [[ "$DO_CONFIGURE" == "0" && ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
  echo -e "${RED}Build directory not configured:${NC} $BUILD_DIR" >&2
  echo "Remove --no-configure or set --build-dir/YAZE_BUILD_DIR to an existing build." >&2
  exit 2
fi

if [[ "$DO_CONFIGURE" == "1" ]]; then
  echo -e "${YELLOW}→${NC} Configuring (${PRESET})..."
  run_quiet_or_die "Configure" cmake --preset "$PRESET"
  echo -e "${GREEN}✓${NC} Configure ok"
fi

if [[ "$DO_BUILD" == "1" ]]; then
  echo -e "${YELLOW}→${NC} Building test targets..."
  build_targets=()
  if [[ "$MODE" == "quick" ]]; then
    if [[ "$RUN_UNIT" == "1" ]]; then
      build_targets+=(yaze_test_quick_unit_core yaze_test_quick_unit_editor)
    fi
    [[ "$RUN_INTEGRATION" == "1" ]] && build_targets+=(yaze_test_quick_integration)
  else
    [[ "$RUN_UNIT" == "1" ]] && build_targets+=(yaze_test_unit)
    [[ "$RUN_INTEGRATION" == "1" ]] && build_targets+=(yaze_test_integration)
    if [[ "$MODE" == "full" ]]; then
      build_targets=(yaze_test_unit yaze_test_integration)
    fi
  fi
  build_args=(--build "$BUILD_DIR" --target "${build_targets[@]}" --parallel "$JOBS")
  if is_multi_config; then
    build_args+=(--config "$CONFIG")
  fi
  run_quiet_or_die "Build" cmake "${build_args[@]}"
  echo -e "${GREEN}✓${NC} Build ok"
fi

ctest_common=(--test-dir "$BUILD_DIR" --output-on-failure --stop-on-failure -j "$JOBS")
if is_multi_config; then
  ctest_common+=(-C "$CONFIG")
fi

ctest_filter_args=()
if [[ -n "$CTEST_REGEX" ]]; then
  ctest_filter_args+=(-R "$CTEST_REGEX")
fi

if [[ "$MODE" == "full" ]]; then
  if [[ "$LIST_ONLY" == "1" ]]; then
    echo -e "${YELLOW}→${NC} Listing stable tests (ctest -N -L stable)..."
    ctest "${ctest_common[@]}" -N -L stable "${ctest_filter_args[@]}"
  else
    echo -e "${YELLOW}→${NC} Running full stable suite via ctest (-L stable)..."
    ctest "${ctest_common[@]}" -L stable "${ctest_filter_args[@]}"
  fi
  exit 0
fi

if [[ "$MODE" == "quick" ]]; then
  label="quick"
  if [[ "$RUN_UNIT" == "1" && "$RUN_INTEGRATION" == "0" ]]; then
    label="fast_unit"
  elif [[ "$RUN_UNIT" == "0" && "$RUN_INTEGRATION" == "1" ]]; then
    label="fast_integration"
  fi

  if [[ "$LIST_ONLY" == "1" ]]; then
    echo -e "${YELLOW}→${NC} Listing quick tests (ctest -N -L ${label})..."
    ctest "${ctest_common[@]}" -N -L "${label}" "${ctest_filter_args[@]}"
  else
    echo -e "${YELLOW}→${NC} Running quick suites via ctest (-L ${label})..."
    ctest "${ctest_common[@]}" -L "${label}" "${ctest_filter_args[@]}"
  fi
  exit 0
fi

if [[ "$RUN_UNIT" == "1" ]]; then
  echo -e "${YELLOW}→${NC} Unit subset (ctest -R):"
  echo "  $UNIT_REGEX"
  if [[ "$LIST_ONLY" == "1" ]]; then
    ctest "${ctest_common[@]}" -N -R "$UNIT_REGEX"
  else
    ctest "${ctest_common[@]}" -R "$UNIT_REGEX"
    echo -e "${GREEN}✓${NC} Unit subset ok"
  fi
  echo ""
fi

if [[ "$RUN_INTEGRATION" == "1" ]]; then
  echo -e "${YELLOW}→${NC} Integration subset (ctest -R):"
  echo "  $INTEGRATION_REGEX"
  if [[ "$LIST_ONLY" == "1" ]]; then
    ctest "${ctest_common[@]}" -N -R "$INTEGRATION_REGEX"
  else
    ctest "${ctest_common[@]}" -R "$INTEGRATION_REGEX"
    echo -e "${GREEN}✓${NC} Integration subset ok"
  fi
  echo ""
fi

if [[ "$LIST_ONLY" == "1" ]]; then
  exit 0
fi

echo -e "${GREEN}✓ fast loop complete${NC}"
echo ""
echo "Tip: run full stable suite with:"
echo "  $0 --full"
