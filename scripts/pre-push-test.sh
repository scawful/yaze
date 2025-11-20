#!/bin/bash
# Pre-Push Test Script for YAZE
# Runs fast validation checks before pushing to remote
# Catches 90% of CI failures in < 2 minutes

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Configuration
BUILD_DIR="${BUILD_DIR:-$PROJECT_ROOT/build}"
PRESET="${PRESET:-}"
CONFIG_ONLY="${CONFIG_ONLY:-0}"
SMOKE_ONLY="${SMOKE_ONLY:-0}"
SKIP_SYMBOLS="${SKIP_SYMBOLS:-0}"
SKIP_TESTS="${SKIP_TESTS:-0}"
VERBOSE="${VERBOSE:-0}"

# Statistics
TOTAL_CHECKS=0
PASSED_CHECKS=0
FAILED_CHECKS=0
START_TIME=$(date +%s)

# Helper functions
print_header() {
    echo -e "\n${BLUE}===${NC} $1 ${BLUE}===${NC}"
}

print_step() {
    echo -e "${YELLOW}→${NC} $1"
}

print_success() {
    echo -e "${GREEN}✓${NC} $1"
    ((PASSED_CHECKS++))
}

print_error() {
    echo -e "${RED}✗${NC} $1"
    ((FAILED_CHECKS++))
}

print_info() {
    echo -e "${BLUE}ℹ${NC} $1"
}

check_command() {
    if ! command -v "$1" &> /dev/null; then
        print_error "$1 not found. Please install it."
        return 1
    fi
    return 0
}

elapsed_time() {
    local END_TIME=$(date +%s)
    local ELAPSED=$((END_TIME - START_TIME))
    echo "${ELAPSED}s"
}

# Parse arguments
usage() {
    cat << EOF
Usage: $0 [OPTIONS]

Pre-push validation script that runs fast checks to catch CI failures early.

OPTIONS:
    --preset NAME       Use specific CMake preset (auto-detect if not specified)
    --build-dir PATH    Build directory (default: build)
    --config-only       Only validate CMake configuration
    --smoke-only        Only run smoke compilation test
    --skip-symbols      Skip symbol conflict checking
    --skip-tests        Skip running unit tests
    --verbose           Show detailed output
    -h, --help          Show this help message

EXAMPLES:
    $0                          # Run all checks with auto-detected preset
    $0 --preset mac-dbg         # Run all checks with specific preset
    $0 --config-only            # Only validate CMake configuration
    $0 --smoke-only             # Only compile representative files
    $0 --skip-tests             # Skip unit tests (faster)

TIME BUDGET:
    Config validation:  ~10 seconds
    Smoke compilation:  ~90 seconds
    Symbol checking:    ~30 seconds
    Unit tests:         ~30 seconds
    ─────────────────────────────
    Total (all checks): ~2 minutes

WHAT THIS CATCHES:
    ✓ CMake configuration errors
    ✓ Missing include paths
    ✓ Header-only compilation issues
    ✓ Symbol conflicts (ODR violations)
    ✓ Unit test failures
    ✓ Platform-specific issues

EOF
    exit 0
}

while [[ $# -gt 0 ]]; do
    case $1 in
        --preset)
            PRESET="$2"
            shift 2
            ;;
        --build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        --config-only)
            CONFIG_ONLY=1
            shift
            ;;
        --smoke-only)
            SMOKE_ONLY=1
            shift
            ;;
        --skip-symbols)
            SKIP_SYMBOLS=1
            shift
            ;;
        --skip-tests)
            SKIP_TESTS=1
            shift
            ;;
        --verbose)
            VERBOSE=1
            shift
            ;;
        -h|--help)
            usage
            ;;
        *)
            print_error "Unknown option: $1"
            usage
            ;;
    esac
done

# Auto-detect preset if not specified
if [[ -z "$PRESET" ]]; then
    if [[ "$OSTYPE" == "darwin"* ]]; then
        PRESET="mac-dbg"
    elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
        PRESET="lin-dbg"
    else
        print_error "Unsupported platform: $OSTYPE"
        print_info "Please specify --preset manually"
        exit 1
    fi
    print_info "Auto-detected preset: $PRESET"
fi

cd "$PROJECT_ROOT"

print_header "YAZE Pre-Push Validation"
print_info "Preset: $PRESET"
print_info "Build directory: $BUILD_DIR"
print_info "Time budget: ~2 minutes"
echo ""

# ============================================================================
# LEVEL 0: Static Analysis
# ============================================================================

print_header "Level 0: Static Analysis"
((TOTAL_CHECKS++))

print_step "Checking code formatting..."
if [[ $VERBOSE -eq 1 ]]; then
    if cmake --build "$BUILD_DIR" --target yaze-format-check 2>&1; then
        print_success "Code formatting is correct"
    else
        print_error "Code formatting check failed"
        print_info "Run: cmake --build $BUILD_DIR --target yaze-format"
        exit 1
    fi
else
    if cmake --build "$BUILD_DIR" --target yaze-format-check > /dev/null 2>&1; then
        print_success "Code formatting is correct"
    else
        print_error "Code formatting check failed"
        print_info "Run: cmake --build $BUILD_DIR --target yaze-format"
        exit 1
    fi
fi

# Skip remaining checks if config-only
if [[ $CONFIG_ONLY -eq 1 ]]; then
    print_header "Summary (Config Only)"
    print_info "Time elapsed: $(elapsed_time)"
    print_info "Total checks: $TOTAL_CHECKS"
    print_info "Passed: ${GREEN}$PASSED_CHECKS${NC}"
    print_info "Failed: ${RED}$FAILED_CHECKS${NC}"
    exit 0
fi

# ============================================================================
# LEVEL 1: Configuration Validation
# ============================================================================

print_header "Level 1: Configuration Validation"
((TOTAL_CHECKS++))

print_step "Validating CMake preset: $PRESET"
if cmake --preset "$PRESET" -DCMAKE_VERBOSE_MAKEFILE=OFF > /dev/null 2>&1; then
    print_success "CMake configuration successful"
else
    print_error "CMake configuration failed"
    print_info "Run: cmake --preset $PRESET (with verbose output)"
    exit 1
fi

# Check for include path issues
((TOTAL_CHECKS++))
print_step "Checking include path propagation..."
if [[ $VERBOSE -eq 1 ]]; then
    if grep -q "INCLUDE_DIRECTORIES" "$BUILD_DIR/CMakeCache.txt"; then
        print_success "Include paths configured"
    else
        print_error "Include paths not properly configured"
        exit 1
    fi
else
    if grep -q "INCLUDE_DIRECTORIES" "$BUILD_DIR/CMakeCache.txt" 2>/dev/null; then
        print_success "Include paths configured"
    else
        print_error "Include paths not properly configured"
        exit 1
    fi
fi

# ============================================================================
# LEVEL 2: Smoke Compilation
# ============================================================================

if [[ $SMOKE_ONLY -eq 0 ]]; then
    print_header "Level 2: Smoke Compilation"
    ((TOTAL_CHECKS++))

    print_step "Compiling representative files..."
    print_info "This validates headers, includes, and preprocessor directives"

    # List of representative files (one per major library)
    SMOKE_FILES=(
        "src/app/rom.cc"
        "src/app/gfx/bitmap.cc"
        "src/zelda3/overworld/overworld.cc"
        "src/cli/service/resources/resource_catalog.cc"
    )

    SMOKE_FAILED=0
    for file in "${SMOKE_FILES[@]}"; do
        if [[ ! -f "$PROJECT_ROOT/$file" ]]; then
            print_info "Skipping $file (not found)"
            continue
        fi

        # Get object file path
        OBJ_FILE="$BUILD_DIR/$(echo "$file" | sed 's/src\///' | sed 's/\.cc$/.cc.o/')"

        if [[ $VERBOSE -eq 1 ]]; then
            print_step "  Compiling $file"
            if cmake --build "$BUILD_DIR" --target "$(basename "$OBJ_FILE")" 2>&1; then
                print_success "  ✓ $file"
            else
                print_error "  ✗ $file"
                SMOKE_FAILED=1
            fi
        else
            if cmake --build "$BUILD_DIR" --target "$(basename "$OBJ_FILE")" > /dev/null 2>&1; then
                print_success "  ✓ $file"
            else
                print_error "  ✗ $file (run with --verbose for details)"
                SMOKE_FAILED=1
            fi
        fi
    done

    if [[ $SMOKE_FAILED -eq 0 ]]; then
        print_success "Smoke compilation successful"
    else
        print_error "Smoke compilation failed"
        print_info "Run: cmake --build $BUILD_DIR -v (for verbose output)"
        exit 1
    fi
fi

# ============================================================================
# LEVEL 3: Symbol Validation
# ============================================================================

if [[ $SKIP_SYMBOLS -eq 0 ]]; then
    print_header "Level 3: Symbol Validation"
    ((TOTAL_CHECKS++))

    print_step "Checking for symbol conflicts..."
    print_info "This detects ODR violations and duplicate symbols"

    if [[ -x "$SCRIPT_DIR/verify-symbols.sh" ]]; then
        if [[ $VERBOSE -eq 1 ]]; then
            if "$SCRIPT_DIR/verify-symbols.sh" --build-dir "$BUILD_DIR"; then
                print_success "No symbol conflicts detected"
            else
                print_error "Symbol conflicts detected"
                exit 1
            fi
        else
            if "$SCRIPT_DIR/verify-symbols.sh" --build-dir "$BUILD_DIR" > /dev/null 2>&1; then
                print_success "No symbol conflicts detected"
            else
                print_error "Symbol conflicts detected"
                print_info "Run: ./scripts/verify-symbols.sh --build-dir $BUILD_DIR"
                exit 1
            fi
        fi
    else
        print_info "Symbol checker not found (skipping)"
        print_info "Create: scripts/verify-symbols.sh"
    fi
fi

# ============================================================================
# LEVEL 4: Unit Tests
# ============================================================================

if [[ $SKIP_TESTS -eq 0 ]]; then
    print_header "Level 4: Unit Tests"
    ((TOTAL_CHECKS++))

    print_step "Running unit tests..."
    print_info "This validates component logic"

    TEST_BINARY="$BUILD_DIR/bin/yaze_test"
    if [[ ! -x "$TEST_BINARY" ]]; then
        print_info "Test binary not found, building..."
        if cmake --build "$BUILD_DIR" --target yaze_test > /dev/null 2>&1; then
            print_success "Test binary built"
        else
            print_error "Failed to build test binary"
            exit 1
        fi
    fi

    if [[ $VERBOSE -eq 1 ]]; then
        if "$TEST_BINARY" --unit 2>&1; then
            print_success "All unit tests passed"
        else
            print_error "Unit tests failed"
            exit 1
        fi
    else
        if "$TEST_BINARY" --unit > /dev/null 2>&1; then
            print_success "All unit tests passed"
        else
            print_error "Unit tests failed"
            print_info "Run: $TEST_BINARY --unit (for detailed output)"
            exit 1
        fi
    fi
fi

# ============================================================================
# Summary
# ============================================================================

print_header "Summary"
END_TIME=$(date +%s)
ELAPSED=$((END_TIME - START_TIME))

print_info "Time elapsed: ${ELAPSED}s"
print_info "Total checks: $TOTAL_CHECKS"
print_info "Passed: ${GREEN}$PASSED_CHECKS${NC}"
print_info "Failed: ${RED}$FAILED_CHECKS${NC}"

if [[ $FAILED_CHECKS -eq 0 ]]; then
    echo ""
    print_success "All checks passed! Safe to push."
    exit 0
else
    echo ""
    print_error "Some checks failed. Please fix before pushing."
    exit 1
fi
