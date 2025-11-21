#!/bin/bash
# CMake Preset Configuration Tester
# Tests that all CMake presets can configure successfully
#
# Usage:
#   ./scripts/test-cmake-presets.sh [OPTIONS]
#
# Options:
#   --parallel N       Test N presets in parallel (default: 4)
#   --preset PRESET    Test only specific preset
#   --platform PLATFORM Test only presets for platform (mac, win, lin)
#   --quick           Skip cleaning between tests
#   --verbose         Show full CMake output
#
# Exit codes:
#   0 - All presets configured successfully
#   1 - One or more presets failed

set -euo pipefail

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Default options
PARALLEL_JOBS=4
SPECIFIC_PRESET=""
PLATFORM_FILTER=""
QUICK_MODE=false
VERBOSE=false

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --parallel)
            PARALLEL_JOBS="$2"
            shift 2
            ;;
        --preset)
            SPECIFIC_PRESET="$2"
            shift 2
            ;;
        --platform)
            PLATFORM_FILTER="$2"
            shift 2
            ;;
        --quick)
            QUICK_MODE=true
            shift
            ;;
        --verbose)
            VERBOSE=true
            shift
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Detect current platform
detect_platform() {
    case "$(uname -s)" in
        Darwin*)
            echo "mac"
            ;;
        Linux*)
            echo "lin"
            ;;
        MINGW*|MSYS*|CYGWIN*)
            echo "win"
            ;;
        *)
            echo "unknown"
            ;;
    esac
}

CURRENT_PLATFORM=$(detect_platform)

# Get list of presets from CMakePresets.json
get_presets() {
    if [ ! -f "CMakePresets.json" ]; then
        echo -e "${RED}✗ CMakePresets.json not found${NC}"
        exit 1
    fi

    # Use jq if available, otherwise use grep
    if command -v jq &> /dev/null; then
        jq -r '.configurePresets[] | select(.hidden != true) | .name' CMakePresets.json
    else
        # Fallback to grep-based extraction
        grep -A 1 '"name":' CMakePresets.json | grep -v '"hidden": true' | grep '"name"' | cut -d'"' -f4
    fi
}

# Filter presets based on criteria
filter_presets() {
    local presets="$1"
    local filtered=""

    if [ -n "$SPECIFIC_PRESET" ]; then
        echo "$SPECIFIC_PRESET"
        return
    fi

    for preset in $presets; do
        # Skip hidden/base presets
        if [[ "$preset" == *"base"* ]]; then
            continue
        fi

        # Filter by platform if specified
        if [ -n "$PLATFORM_FILTER" ]; then
            if [[ ! "$preset" == *"$PLATFORM_FILTER"* ]]; then
                continue
            fi
        fi

        # Skip platform-specific presets if not on that platform
        if [ "$CURRENT_PLATFORM" != "unknown" ]; then
            if [[ "$preset" == mac-* ]] && [ "$CURRENT_PLATFORM" != "mac" ]; then
                continue
            fi
            if [[ "$preset" == win-* ]] && [ "$CURRENT_PLATFORM" != "win" ]; then
                continue
            fi
            if [[ "$preset" == lin-* ]] && [ "$CURRENT_PLATFORM" != "lin" ]; then
                continue
            fi
        fi

        filtered="$filtered $preset"
    done

    echo "$filtered"
}

# Test a single preset
test_preset() {
    local preset="$1"
    local build_dir="build_preset_test_${preset}"
    local log_file="preset_test_${preset}.log"

    echo -e "${CYAN}Testing preset: $preset${NC}"

    # Clean build directory unless in quick mode
    if [ "$QUICK_MODE" = false ] && [ -d "$build_dir" ]; then
        rm -rf "$build_dir"
    fi

    # Configure preset
    local start_time=$(date +%s)

    if [ "$VERBOSE" = true ]; then
        if cmake --preset "$preset" -B "$build_dir" 2>&1 | tee "$log_file"; then
            local end_time=$(date +%s)
            local duration=$((end_time - start_time))
            echo -e "${GREEN}✓${NC} $preset configured successfully (${duration}s)"
            rm -f "$log_file"
            return 0
        else
            local end_time=$(date +%s)
            local duration=$((end_time - start_time))
            echo -e "${RED}✗${NC} $preset failed (${duration}s)"
            echo "  Log saved to: $log_file"
            return 1
        fi
    else
        if cmake --preset "$preset" -B "$build_dir" > "$log_file" 2>&1; then
            local end_time=$(date +%s)
            local duration=$((end_time - start_time))
            echo -e "${GREEN}✓${NC} $preset configured successfully (${duration}s)"
            rm -f "$log_file"
            return 0
        else
            local end_time=$(date +%s)
            local duration=$((end_time - start_time))
            echo -e "${RED}✗${NC} $preset failed (${duration}s)"
            echo "  Log saved to: $log_file"
            return 1
        fi
    fi
}

# Main execution
main() {
    echo -e "${BLUE}=== CMake Preset Configuration Tester ===${NC}"
    echo "Platform: $CURRENT_PLATFORM"
    echo "Parallel jobs: $PARALLEL_JOBS"
    echo ""

    # Get and filter presets
    all_presets=$(get_presets)
    test_presets=$(filter_presets "$all_presets")

    if [ -z "$test_presets" ]; then
        echo -e "${YELLOW}⚠ No presets to test${NC}"
        exit 0
    fi

    echo -e "${BLUE}Presets to test:${NC}"
    for preset in $test_presets; do
        echo "  - $preset"
    done
    echo ""

    # Test presets
    local total=0
    local passed=0
    local failed=0
    local failed_presets=""

    # Export function for parallel execution
    export -f test_preset
    export VERBOSE
    export QUICK_MODE
    export RED GREEN YELLOW BLUE CYAN NC

    if [ "$PARALLEL_JOBS" -gt 1 ]; then
        echo -e "${BLUE}Running tests in parallel (jobs: $PARALLEL_JOBS)...${NC}\n"

        # Use GNU parallel if available, otherwise use xargs
        if command -v parallel &> /dev/null; then
            echo "$test_presets" | tr ' ' '\n' | parallel -j "$PARALLEL_JOBS" test_preset
        else
            echo "$test_presets" | tr ' ' '\n' | xargs -P "$PARALLEL_JOBS" -I {} bash -c "$(declare -f test_preset); test_preset {}"
        fi

        # Collect results
        for preset in $test_presets; do
            total=$((total + 1))
            if [ -f "preset_test_${preset}.log" ]; then
                failed=$((failed + 1))
                failed_presets="$failed_presets\n  - $preset"
            else
                passed=$((passed + 1))
            fi
        done
    else
        echo -e "${BLUE}Running tests sequentially...${NC}\n"

        for preset in $test_presets; do
            total=$((total + 1))
            if test_preset "$preset"; then
                passed=$((passed + 1))
            else
                failed=$((failed + 1))
                failed_presets="$failed_presets\n  - $preset"
            fi
        done
    fi

    # Summary
    echo ""
    echo -e "${BLUE}=== Test Summary ===${NC}"
    echo "Total presets tested: $total"
    echo -e "${GREEN}Passed: $passed${NC}"

    if [ $failed -gt 0 ]; then
        echo -e "${RED}Failed: $failed${NC}"
        echo -e "${RED}Failed presets:${NC}$failed_presets"
        echo ""
        echo "Check log files for details: preset_test_*.log"
        exit 1
    else
        echo -e "${GREEN}✓ All presets configured successfully!${NC}"

        # Cleanup test build directories
        if [ "$QUICK_MODE" = false ]; then
            echo "Cleaning up test build directories..."
            rm -rf build_preset_test_*
        fi

        exit 0
    fi
}

# Run main
main
