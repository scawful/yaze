#!/usr/bin/env bash
# Pre-push validation script for yaze
# Runs fast checks before pushing to catch common issues early
#
# Usage:
#   scripts/pre-push.sh [--skip-tests] [--skip-format]
#
# Options:
#   --skip-tests    Skip running unit tests
#   --skip-format   Skip code formatting check
#   --skip-build    Skip build verification
#   --help          Show this help message
#
# Exit codes:
#   0 - All checks passed
#   1 - Build failed
#   2 - Tests failed
#   3 - Format check failed
#   4 - Configuration error

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
SKIP_TESTS=false
SKIP_FORMAT=false
SKIP_BUILD=false
BUILD_DIR="build"
TEST_TIMEOUT=120  # 2 minutes max for tests

# Parse command line arguments
while [[ $# -gt 0 ]]; do
  case $1 in
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
    --help)
      grep '^#' "$0" | sed 's/^# \?//'
      exit 0
      ;;
    *)
      echo -e "${RED}❌ Unknown option: $1${NC}"
      echo "Use --help for usage information"
      exit 4
      ;;
  esac
done

# Helper functions
print_header() {
  echo -e "\n${BLUE}===${NC} $1 ${BLUE}===${NC}"
}

print_success() {
  echo -e "${GREEN}✅ $1${NC}"
}

print_error() {
  echo -e "${RED}❌ $1${NC}"
}

print_warning() {
  echo -e "${YELLOW}⚠️  $1${NC}"
}

print_info() {
  echo -e "${BLUE}ℹ️  $1${NC}"
}

# Detect platform
detect_platform() {
  case "$(uname -s)" in
    Darwin*) echo "mac" ;;
    Linux*)  echo "lin" ;;
    MINGW*|MSYS*|CYGWIN*) echo "win" ;;
    *) echo "unknown" ;;
  esac
}

# Get appropriate preset for platform
get_preset() {
  local platform=$1
  if [ -f "$BUILD_DIR/CMakeCache.txt" ]; then
    # Extract preset from existing build
    grep "CMAKE_PROJECT_NAME" "$BUILD_DIR/CMakeCache.txt" >/dev/null 2>&1 && echo "existing" && return
  fi

  # Use platform default debug preset
  echo "${platform}-dbg"
}

# Check if CMake is configured
check_cmake_configured() {
  if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
    print_warning "Build directory not configured"
    print_info "Run: cmake --preset <preset> to configure"
    return 1
  fi
  return 0
}

# Main script
main() {
  print_header "Pre-Push Validation"

  local platform
  platform=$(detect_platform)
  print_info "Detected platform: $platform"

  # Check for git repository
  if ! git rev-parse --git-dir > /dev/null 2>&1; then
    print_error "Not in a git repository"
    exit 4
  fi

  local start_time
  start_time=$(date +%s)

  # 1. Build Verification
  if [ "$SKIP_BUILD" = false ]; then
    print_header "Step 1/3: Build Verification"

    if ! check_cmake_configured; then
      print_error "Build not configured. Skipping build check."
      print_info "Configure with: cmake --preset ${platform}-dbg"
      exit 4
    fi

    print_info "Building yaze_test target..."
    if ! cmake --build "$BUILD_DIR" --target yaze_test 2>&1 | tail -20; then
      print_error "Build failed!"
      print_info "Fix build errors and try again"
      exit 1
    fi
    print_success "Build passed"
  else
    print_warning "Skipping build verification (--skip-build)"
  fi

  # 2. Unit Tests
  if [ "$SKIP_TESTS" = false ]; then
    print_header "Step 2/3: Unit Tests"

    local test_binary="$BUILD_DIR/bin/yaze_test"
    if [ ! -f "$test_binary" ]; then
      print_error "Test binary not found: $test_binary"
      print_info "Build tests first: cmake --build $BUILD_DIR --target yaze_test"
      exit 2
    fi

    print_info "Running unit tests (timeout: ${TEST_TIMEOUT}s)..."
    if ! timeout "$TEST_TIMEOUT" "$test_binary" --unit --gtest_brief=1 2>&1; then
      print_error "Unit tests failed!"
      print_info "Run tests manually to see details: $test_binary --unit"
      exit 2
    fi
    print_success "Unit tests passed"
  else
    print_warning "Skipping unit tests (--skip-tests)"
  fi

  # 3. Code Formatting
  if [ "$SKIP_FORMAT" = false ]; then
    print_header "Step 3/3: Code Formatting"

    # Check if format-check target exists
    if cmake --build "$BUILD_DIR" --target help 2>/dev/null | grep -q "format-check"; then
      print_info "Checking code formatting..."
      if ! cmake --build "$BUILD_DIR" --target format-check 2>&1 | tail -10; then
        print_error "Code formatting check failed!"
        print_info "Fix with: cmake --build $BUILD_DIR --target format"
        exit 3
      fi
      print_success "Code formatting passed"
    else
      print_warning "format-check target not available, skipping"
    fi
  else
    print_warning "Skipping format check (--skip-format)"
  fi

  # Summary
  local end_time
  end_time=$(date +%s)
  local duration=$((end_time - start_time))

  print_header "Pre-Push Validation Complete"
  print_success "All checks passed in ${duration}s"
  print_info "Safe to push!"

  return 0
}

# Run main function
main "$@"
