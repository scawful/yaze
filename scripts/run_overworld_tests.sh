#!/bin/bash

# =============================================================================
# YAZE Overworld Comprehensive Test Runner
# =============================================================================
# 
# This script orchestrates the complete overworld testing workflow:
# 1. Builds the golden data extractor tool
# 2. Extracts golden data from vanilla and modified ROMs
# 3. Runs unit tests, integration tests, and E2E tests
# 4. Generates comprehensive test reports
# 5. Validates before/after edit states
# 
# Usage: ./scripts/run_overworld_tests.sh [rom_path] [options]
# 
# Options:
#   --skip-build          Skip building the golden data extractor
#   --skip-golden-data    Skip golden data extraction
#   --skip-unit-tests     Skip unit tests
#   --skip-integration    Skip integration tests
#   --skip-e2e           Skip E2E tests
#   --generate-report    Generate detailed test report
#   --cleanup            Clean up test files after completion
# =============================================================================

set -e  # Exit on any error

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
BIN_DIR="$BUILD_DIR/bin"
TEST_DIR="$PROJECT_ROOT/test"

# Default values
ROM_PATH=""
SKIP_BUILD=false
SKIP_GOLDEN_DATA=false
SKIP_UNIT_TESTS=false
SKIP_INTEGRATION=false
SKIP_E2E=false
GENERATE_REPORT=false
CLEANUP=false

# Test results
UNIT_TEST_RESULTS=""
INTEGRATION_TEST_RESULTS=""
E2E_TEST_RESULTS=""
GOLDEN_DATA_STATUS=""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Helper functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

show_usage() {
    cat << EOF
Usage: $0 [rom_path] [options]

Arguments:
  rom_path              Path to the ROM file to test with

Options:
  --skip-build          Skip building the golden data extractor
  --skip-golden-data    Skip golden data extraction
  --skip-unit-tests     Skip unit tests
  --skip-integration    Skip integration tests
  --skip-e2e           Skip E2E tests
  --generate-report    Generate detailed test report
  --cleanup            Clean up test files after completion

Examples:
  $0 roms/alttp_vanilla.sfc
  $0 roms/alttp_vanilla.sfc --generate-report --cleanup
  $0 /path/to/alttp_vanilla.sfc --skip-unit-tests
EOF
}

parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            --skip-build)
                SKIP_BUILD=true
                shift
                ;;
            --skip-golden-data)
                SKIP_GOLDEN_DATA=true
                shift
                ;;
            --skip-unit-tests)
                SKIP_UNIT_TESTS=true
                shift
                ;;
            --skip-integration)
                SKIP_INTEGRATION=true
                shift
                ;;
            --skip-e2e)
                SKIP_E2E=true
                shift
                ;;
            --generate-report)
                GENERATE_REPORT=true
                shift
                ;;
            --cleanup)
                CLEANUP=true
                shift
                ;;
            --help|-h)
                show_usage
                exit 0
                ;;
            -*)
                log_error "Unknown option: $1"
                show_usage
                exit 1
                ;;
            *)
                if [[ -z "$ROM_PATH" ]]; then
                    ROM_PATH="$1"
                else
                    log_error "Multiple ROM paths specified"
                    exit 1
                fi
                shift
                ;;
        esac
    done
}

check_prerequisites() {
    log_info "Checking prerequisites..."
    
    # Check if we're in the right directory
    if [[ ! -f "$PROJECT_ROOT/CMakeLists.txt" ]]; then
        log_error "Not in YAZE project root directory"
        exit 1
    fi
    
    # Check if build directory exists
    if [[ ! -d "$BUILD_DIR" ]]; then
        log_warning "Build directory not found. Run 'cmake --build build' first."
        exit 1
    fi
    
    # Check if ROM path is provided
    if [[ -z "$ROM_PATH" ]]; then
        log_error "ROM path is required"
        show_usage
        exit 1
    fi
    
    # Check if ROM file exists
    if [[ ! -f "$ROM_PATH" ]]; then
        log_error "ROM file not found: $ROM_PATH"
        exit 1
    fi
    
    log_success "Prerequisites check passed"
}

build_golden_data_extractor() {
    if [[ "$SKIP_BUILD" == true ]]; then
        log_info "Skipping golden data extractor build"
        return
    fi
    
    log_info "Building golden data extractor..."
    
    cd "$PROJECT_ROOT"
    
    # Build the golden data extractor
    if cmake --build "$BUILD_DIR" --target overworld_golden_data_extractor; then
        log_success "Golden data extractor built successfully"
    else
        log_error "Failed to build golden data extractor"
        exit 1
    fi
}

extract_golden_data() {
    if [[ "$SKIP_GOLDEN_DATA" == true ]]; then
        log_info "Skipping golden data extraction"
        return
    fi
    
    log_info "Extracting golden data from ROM..."
    
    local golden_data_file="$TEST_DIR/golden_data/$(basename "$ROM_PATH" .sfc)_golden_data.h"
    
    # Create golden data directory if it doesn't exist
    mkdir -p "$(dirname "$golden_data_file")"
    
    # Run the golden data extractor
    if "$BIN_DIR/overworld_golden_data_extractor" "$ROM_PATH" "$golden_data_file"; then
        log_success "Golden data extracted to: $golden_data_file"
        GOLDEN_DATA_STATUS="SUCCESS: $golden_data_file"
    else
        log_error "Failed to extract golden data"
        GOLDEN_DATA_STATUS="FAILED"
        return 1
    fi
}

run_unit_tests() {
    if [[ "$SKIP_UNIT_TESTS" == true ]]; then
        log_info "Skipping unit tests"
        return
    fi
    
    log_info "Running unit tests..."
    
    cd "$PROJECT_ROOT"
    
    # Set environment variable for ROM path
    export YAZE_TEST_ROM_VANILLA="$ROM_PATH"
    export YAZE_TEST_ROM_PATH="$ROM_PATH"
    
    # Run unit tests
    if ctest --test-dir "$BUILD_DIR" --output-on-failure --verbose \
             --tests-regex "overworld.*test|extract.*vanilla.*values"; then
        log_success "Unit tests passed"
        UNIT_TEST_RESULTS="PASSED"
    else
        log_error "Unit tests failed"
        UNIT_TEST_RESULTS="FAILED"
        return 1
    fi
}

run_integration_tests() {
    if [[ "$SKIP_INTEGRATION" == true ]]; then
        log_info "Skipping integration tests"
        return
    fi
    
    log_info "Running integration tests..."
    
    cd "$PROJECT_ROOT"
    
    # Set environment variable for ROM path
    export YAZE_TEST_ROM_VANILLA="$ROM_PATH"
    export YAZE_TEST_ROM_PATH="$ROM_PATH"
    
    # Run integration tests
    if ctest --test-dir "$BUILD_DIR" --output-on-failure --verbose \
             --tests-regex "overworld.*integration"; then
        log_success "Integration tests passed"
        INTEGRATION_TEST_RESULTS="PASSED"
    else
        log_error "Integration tests failed"
        INTEGRATION_TEST_RESULTS="FAILED"
        return 1
    fi
}

run_e2e_tests() {
    if [[ "$SKIP_E2E" == true ]]; then
        log_info "Skipping E2E tests"
        return
    fi
    
    log_info "Running E2E tests..."
    
    cd "$PROJECT_ROOT"
    
    # Set environment variable for ROM path
    export YAZE_TEST_ROM_VANILLA="$ROM_PATH"
    export YAZE_TEST_ROM_PATH="$ROM_PATH"
    
    # Run E2E tests
    if ctest --test-dir "$BUILD_DIR" --output-on-failure --verbose \
             --tests-regex "overworld.*e2e"; then
        log_success "E2E tests passed"
        E2E_TEST_RESULTS="PASSED"
    else
        log_error "E2E tests failed"
        E2E_TEST_RESULTS="FAILED"
        return 1
    fi
}

generate_test_report() {
    if [[ "$GENERATE_REPORT" != true ]]; then
        return
    fi
    
    log_info "Generating test report..."
    
    local report_file="$TEST_DIR/reports/overworld_test_report_$(date +%Y%m%d_%H%M%S).md"
    
    # Create reports directory if it doesn't exist
    mkdir -p "$(dirname "$report_file")"
    
    cat > "$report_file" << EOF
# YAZE Overworld Test Report

**Generated:** $(date)
**ROM:** $ROM_PATH
**ROM Size:** $(stat -f%z "$ROM_PATH" 2>/dev/null || stat -c%s "$ROM_PATH") bytes

## Test Results Summary

| Test Category | Status | Details |
|---------------|--------|---------|
| Golden Data Extraction | $GOLDEN_DATA_STATUS | |
| Unit Tests | $UNIT_TEST_RESULTS | |
| Integration Tests | $INTEGRATION_TEST_RESULTS | |
| E2E Tests | $E2E_TEST_RESULTS | |

## Test Configuration

- Skip Build: $SKIP_BUILD
- Skip Golden Data: $SKIP_GOLDEN_DATA
- Skip Unit Tests: $SKIP_UNIT_TESTS
- Skip Integration: $SKIP_INTEGRATION
- Skip E2E: $SKIP_E2E
- Generate Report: $GENERATE_REPORT
- Cleanup: $CLEANUP

## ROM Information

EOF

    # Add ROM header information if available
    if command -v hexdump >/dev/null 2>&1; then
        echo "### ROM Header" >> "$report_file"
        echo '```' >> "$report_file"
        hexdump -C "$ROM_PATH" | head -20 >> "$report_file"
        echo '```' >> "$report_file"
    fi
    
    echo "" >> "$report_file"
    echo "## Next Steps" >> "$report_file"
    echo "" >> "$report_file"
    echo "1. Review test results above" >> "$report_file"
    echo "2. If tests failed, check the logs for details" >> "$report_file"
    echo "3. Run individual test categories for debugging" >> "$report_file"
    echo "4. Update golden data if ROM changes are expected" >> "$report_file"
    
    log_success "Test report generated: $report_file"
}

cleanup_test_files() {
    if [[ "$CLEANUP" != true ]]; then
        return
    fi
    
    log_info "Cleaning up test files..."
    
    # Remove test ROMs created during testing
    find "$TEST_DIR" -name "test_*.sfc" -type f -delete 2>/dev/null || true
    
    # Remove temporary golden data files
    find "$TEST_DIR" -name "*_golden_data.h" -type f -delete 2>/dev/null || true
    
    log_success "Test files cleaned up"
}

main() {
    log_info "Starting YAZE Overworld Comprehensive Test Suite"
    log_info "ROM Path: $ROM_PATH"
    
    # Parse command line arguments
    parse_args "$@"
    
    # Check prerequisites
    check_prerequisites
    
    # Build golden data extractor
    build_golden_data_extractor
    
    # Extract golden data
    extract_golden_data
    
    # Run tests
    run_unit_tests
    run_integration_tests
    run_e2e_tests
    
    # Generate report
    generate_test_report
    
    # Cleanup
    cleanup_test_files
    
    # Final status
    log_info "Test suite completed"
    
    if [[ "$UNIT_TEST_RESULTS" == "PASSED" && 
          "$INTEGRATION_TEST_RESULTS" == "PASSED" && 
          "$E2E_TEST_RESULTS" == "PASSED" ]]; then
        log_success "All tests passed!"
        exit 0
    else
        log_error "Some tests failed. Check the results above."
        exit 1
    fi
}

# Run main function with all arguments
main "$@"
