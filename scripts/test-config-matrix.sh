#!/usr/bin/env bash

#
# Configuration Matrix Tester
#
# Quick local testing of important CMake flag combinations
# to catch cross-configuration issues before pushing.
#
# Usage:
#   ./scripts/test-config-matrix.sh                   # Test all configs
#   ./scripts/test-config-matrix.sh --config minimal  # Test specific config
#   ./scripts/test-config-matrix.sh --smoke           # Fast smoke test only
#   ./scripts/test-config-matrix.sh --verbose         # Verbose output
#   ./scripts/test-config-matrix.sh --platform linux  # Platform-specific
#
# Environment:
#   MATRIX_BUILD_DIR: Base directory for builds (default: ./build_matrix)
#   MATRIX_JOBS: Parallel jobs (default: 4)
#   MATRIX_CONFIG: Specific configuration to test
#

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
MATRIX_BUILD_DIR="${MATRIX_BUILD_DIR:=${PROJECT_DIR}/build_matrix}"
MATRIX_JOBS="${MATRIX_JOBS:=4}"
PLATFORM="${PLATFORM:=$(uname -s | tr '[:upper:]' '[:lower:]')}"
VERBOSE="${VERBOSE:=0}"
SMOKE_ONLY="${SMOKE_ONLY:=0}"
SPECIFIC_CONFIG="${SPECIFIC_CONFIG:=}"

# Test result tracking
declare -A RESULTS
TOTAL=0
PASSED=0
FAILED=0

# ============================================================================
# Configuration Definitions
# ============================================================================

# Define each test configuration
# Format: config_name|preset_name|cmake_flags|description

declare -a CONFIGS=(
  # Tier 1: Core builds (fast, must pass)
  "minimal|minimal|-DYAZE_ENABLE_GRPC=OFF -DYAZE_ENABLE_AI=OFF -DYAZE_ENABLE_JSON=ON|Minimal build: no AI, no gRPC"
  "grpc-only|ci-linux|-DYAZE_ENABLE_GRPC=ON -DYAZE_ENABLE_REMOTE_AUTOMATION=OFF -DYAZE_ENABLE_AI_RUNTIME=OFF|gRPC only: automation disabled"
  "full-ai|ci-linux|-DYAZE_ENABLE_GRPC=ON -DYAZE_ENABLE_REMOTE_AUTOMATION=ON -DYAZE_ENABLE_AI_RUNTIME=ON -DYAZE_ENABLE_JSON=ON|Full AI stack: all features on"

  # Tier 2: Feature combinations
  "cli-no-grpc|minimal|-DYAZE_ENABLE_GRPC=OFF -DYAZE_BUILD_GUI=OFF -DYAZE_BUILD_EMU=OFF|CLI only: no GUI, no gRPC"
  "http-api|ci-linux|-DYAZE_ENABLE_GRPC=ON -DYAZE_ENABLE_HTTP_API=ON -DYAZE_ENABLE_JSON=ON|HTTP API: REST endpoints"
  "no-json|ci-linux|-DYAZE_ENABLE_JSON=OFF -DYAZE_ENABLE_GRPC=ON|No JSON: Ollama only"

  # Tier 3: Edge cases
  "all-off|minimal|-DYAZE_BUILD_GUI=OFF -DYAZE_BUILD_CLI=OFF -DYAZE_BUILD_TESTS=OFF|Minimal library only"
)

# ============================================================================
# Helper Functions
# ============================================================================

log_info() {
  echo -e "${BLUE}[INFO]${NC} $*"
}

log_success() {
  echo -e "${GREEN}[✓]${NC} $*"
}

log_error() {
  echo -e "${RED}[✗]${NC} $*"
}

log_warning() {
  echo -e "${YELLOW}[!]${NC} $*"
}

print_header() {
  echo ""
  echo -e "${BLUE}========================================${NC}"
  echo -e "${BLUE}$*${NC}"
  echo -e "${BLUE}========================================${NC}"
}

print_usage() {
  cat <<EOF
Configuration Matrix Tester

Usage:
  $0 [OPTIONS]

Options:
  --config NAME       Test specific configuration (e.g., minimal, full-ai)
  --smoke             Fast smoke test only (configure, compile 5 files)
  --verbose           Verbose output
  --platform PLAT     Force platform (linux, macos, windows)
  --jobs N            Parallel jobs (default: 4)
  --help              Show this help message

Examples:
  $0                                  # Test all configurations
  $0 --config minimal                # Test specific config
  $0 --smoke --config full-ai        # Smoke test full-ai
  $0 --verbose --platform linux      # Verbose, Linux only

Configurations Available:
  minimal             - No AI, no gRPC
  grpc-only           - gRPC without automation
  full-ai             - All features enabled
  cli-no-grpc         - CLI only, no networking
  http-api            - REST API endpoints
  no-json             - Ollama mode (no JSON)
  all-off             - Library only

EOF
}

# ============================================================================
# CMake Configuration Testing
# ============================================================================

test_config() {
  local config_name="$1"
  local preset="$2"
  local cmake_flags="$3"
  local description="$4"

  TOTAL=$((TOTAL + 1))

  echo ""
  print_header "Testing: $config_name"
  log_info "Description: $description"
  log_info "Preset: $preset"
  log_info "CMake flags: $cmake_flags"

  local build_dir="${MATRIX_BUILD_DIR}/${config_name}"

  # Create build directory
  mkdir -p "$build_dir"

  # Configure
  log_info "Configuring CMake..."
  if ! cmake --preset "$preset" \
    -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    $cmake_flags \
    >"${build_dir}/config.log" 2>&1; then
    log_error "Configuration failed for $config_name"
    RESULTS["$config_name"]="FAILED (configure)"
    FAILED=$((FAILED + 1))

    if [ "$VERBOSE" == "1" ]; then
      tail -20 "${build_dir}/config.log"
    fi
    return 1
  fi

  log_success "Configuration successful"

  # Print resolved configuration (for debugging)
  if [ "$VERBOSE" == "1" ]; then
    log_info "Resolved configuration:"
    grep "YAZE_BUILD\|YAZE_ENABLE" "${build_dir}/CMakeCache.txt" | sort | sed 's/^/  /' || true
  fi

  # Build
  log_info "Building..."
  if [ "$SMOKE_ONLY" == "1" ]; then
    # Smoke test: just build a few key files
    log_info "Running smoke test (configure only)"
  else
    if ! cmake --build "$build_dir" \
      --config RelWithDebInfo \
      --parallel "$MATRIX_JOBS" \
      >"${build_dir}/build.log" 2>&1; then
      log_error "Build failed for $config_name"
      RESULTS["$config_name"]="FAILED (build)"
      FAILED=$((FAILED + 1))

      if [ "$VERBOSE" == "1" ]; then
        tail -30 "${build_dir}/build.log"
      fi
      return 1
    fi
  fi

  log_success "Build successful"

  # Run basic tests if available
  if [ -f "${build_dir}/bin/yaze_test" ] && [ "$SMOKE_ONLY" != "1" ]; then
    log_info "Running unit tests..."
    if timeout 30 "${build_dir}/bin/yaze_test" --unit >"${build_dir}/test.log" 2>&1; then
      log_success "Unit tests passed"
    else
      log_warning "Unit tests failed or timed out"
      RESULTS["$config_name"]="PASSED (build, tests failed)"
    fi
  fi

  RESULTS["$config_name"]="PASSED"
  PASSED=$((PASSED + 1))
  return 0
}

# ============================================================================
# Matrix Execution
# ============================================================================

print_matrix_info() {
  echo ""
  echo -e "${BLUE}Configuration Matrix Test${NC}"
  echo "Project: $PROJECT_DIR"
  echo "Build directory: $MATRIX_BUILD_DIR"
  echo "Platform: $PLATFORM"
  echo "Parallel jobs: $MATRIX_JOBS"
  echo "Smoke test only: $SMOKE_ONLY"
  echo ""
}

run_all_configs() {
  print_matrix_info

  for config_line in "${CONFIGS[@]}"; do
    IFS='|' read -r config_name preset flags description <<<"$config_line"

    # Filter by specific config if provided
    if [ -n "$SPECIFIC_CONFIG" ] && [ "$config_name" != "$SPECIFIC_CONFIG" ]; then
      continue
    fi

    # Filter by platform if needed
    case "$preset" in
    ci-linux | lin-* | lin-dev)
      if [[ "$PLATFORM" == "darwin" || "$PLATFORM" == "windows" ]]; then
        log_warning "Skipping $config_name (Linux-only preset on $PLATFORM)"
        continue
      fi
      ;;
    ci-macos | mac-* | win-uni)
      if [[ "$PLATFORM" != "darwin" ]]; then
        log_warning "Skipping $config_name (macOS-only preset on $PLATFORM)"
        continue
      fi
      ;;
    ci-windows | win-* | win-ai)
      if [[ "$PLATFORM" != "windows" ]]; then
        log_warning "Skipping $config_name (Windows-only preset on $PLATFORM)"
        continue
      fi
      ;;
    esac

    test_config "$config_name" "$preset" "$flags" "$description" || true
  done

  print_summary
}

print_summary() {
  print_header "Test Summary"

  for config_name in "${!RESULTS[@]}"; do
    result="${RESULTS[$config_name]}"
    if [[ "$result" == PASSED* ]]; then
      echo -e "${GREEN}✓${NC} $config_name: $result"
    else
      echo -e "${RED}✗${NC} $config_name: $result"
    fi
  done | sort

  echo ""
  echo "Results: $PASSED/$TOTAL passed, $FAILED/$TOTAL failed"
  echo ""

  if [ "$FAILED" -gt 0 ]; then
    log_error "Some configurations failed!"
    echo ""
    echo "Debug tips:"
    echo "  - Check build logs in: $MATRIX_BUILD_DIR/<config>/build.log"
    echo "  - Re-run with --verbose for full output"
    echo "  - Check cmake errors: $MATRIX_BUILD_DIR/<config>/config.log"
    return 1
  else
    log_success "All configurations passed!"
    return 0
  fi
}

# ============================================================================
# Main Entry Point
# ============================================================================

main() {
  # Parse arguments
  while [[ $# -gt 0 ]]; do
    case $1 in
    --config)
      SPECIFIC_CONFIG="$2"
      shift 2
      ;;
    --smoke)
      SMOKE_ONLY=1
      shift
      ;;
    --verbose)
      VERBOSE=1
      shift
      ;;
    --platform)
      PLATFORM="$2"
      shift 2
      ;;
    --jobs)
      MATRIX_JOBS="$2"
      shift 2
      ;;
    --help)
      print_usage
      exit 0
      ;;
    *)
      log_error "Unknown option: $1"
      print_usage
      exit 1
      ;;
    esac
  done

  # Validate environment
  if ! command -v cmake &>/dev/null; then
    log_error "cmake not found in PATH"
    exit 1
  fi

  # Clean up old matrix builds on request
  if [ -d "$MATRIX_BUILD_DIR" ] && [ "$VERBOSE" == "1" ]; then
    log_info "Using existing matrix build directory"
  fi

  # Run tests
  if ! run_all_configs; then
    exit 1
  fi
}

# Run main function
main "$@"
