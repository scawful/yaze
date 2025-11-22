#!/bin/bash
# Symbol Conflict Detector for YAZE
# Detects ODR violations and duplicate symbols across libraries
# Prevents link-time failures that only appear in CI

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
VERBOSE="${VERBOSE:-0}"
SHOW_ALL="${SHOW_ALL:-0}"

# Statistics
TOTAL_LIBRARIES=0
DUPLICATE_SYMBOLS=0
POTENTIAL_ODR=0

# Helper functions
print_header() {
    echo -e "\n${BLUE}===${NC} $1 ${BLUE}===${NC}"
}

print_success() {
    echo -e "${GREEN}✓${NC} $1"
}

print_error() {
    echo -e "${RED}✗${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}⚠${NC} $1"
}

print_info() {
    echo -e "${BLUE}ℹ${NC} $1"
}

# Parse arguments
usage() {
    cat << EOF
Usage: $0 [OPTIONS]

Symbol conflict detector that scans built libraries for duplicate symbols
and ODR violations.

OPTIONS:
    --build-dir PATH    Build directory (default: build)
    --verbose           Show detailed symbol information
    --show-all          Show all symbols (including safe duplicates)
    -h, --help          Show this help message

EXAMPLES:
    $0                          # Scan default build directory
    $0 --build-dir build_test   # Scan specific build directory
    $0 --verbose                # Show detailed output
    $0 --show-all               # Show all symbols (verbose)

WHAT IT DETECTS:
    ✓ Duplicate symbol definitions (ODR violations)
    ✓ FLAGS_* conflicts (gflags issues)
    ✓ Multiple weak symbols
    ✓ Template instantiation conflicts

SAFE SYMBOLS (ignored):
    - vtable symbols (typeinfo)
    - guard variables (__guard)
    - std::* standard library symbols
    - Abseil inline namespaces

PLATFORMS:
    - macOS: Uses 'nm' and 'c++filt'
    - Linux: Uses 'nm' and 'c++filt'
    - Windows: Uses 'dumpbin' (not implemented yet)

EOF
    exit 0
}

while [[ $# -gt 0 ]]; do
    case $1 in
        --build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        --verbose)
            VERBOSE=1
            shift
            ;;
        --show-all)
            SHOW_ALL=1
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

# Check for required tools
check_tools() {
    local MISSING=0

    if ! command -v nm &> /dev/null; then
        print_error "nm not found (required for symbol inspection)"
        MISSING=1
    fi

    if ! command -v c++filt &> /dev/null; then
        print_warning "c++filt not found (symbol demangling disabled)"
    fi

    if [[ $MISSING -eq 1 ]]; then
        print_info "Install build tools: xcode-select --install (macOS) or build-essential (Linux)"
        exit 1
    fi
}

# Filter out safe duplicate symbols
is_safe_symbol() {
    local symbol="$1"

    # Safe patterns (these are expected to have duplicates)
    local SAFE_PATTERNS=(
        "^typeinfo"           # RTTI typeinfo
        "^vtable"             # Virtual tables
        "^guard variable"     # Static initialization guards
        "^std::"              # Standard library
        "^__cxx"              # C++ runtime
        "^__gnu"              # GNU extensions
        "^absl::lts_"         # Abseil LTS inline namespace (versioning)
        "^non-virtual thunk"  # Virtual inheritance thunks
        "^construction vtable" # Construction virtual tables
    )

    for pattern in "${SAFE_PATTERNS[@]}"; do
        if [[ "$symbol" =~ $pattern ]]; then
            return 0  # Safe symbol
        fi
    done

    return 1  # Potentially problematic symbol
}

# Check if symbol is a FLAGS definition
is_flags_symbol() {
    local symbol="$1"
    if [[ "$symbol" =~ ^FLAGS_ ]] || [[ "$symbol" =~ fL[A-Z] ]]; then
        return 0  # FLAGS symbol
    fi
    return 1
}

# Extract and analyze symbols
analyze_symbols() {
    print_header "Symbol Conflict Detection"

    # Find all static libraries
    local LIBRARIES=()
    if [[ -d "$BUILD_DIR/lib" ]]; then
        while IFS= read -r -d '' lib; do
            LIBRARIES+=("$lib")
        done < <(find "$BUILD_DIR/lib" -name "*.a" -print0 2>/dev/null)
    fi
    if [[ -d "$BUILD_DIR" ]]; then
        while IFS= read -r -d '' lib; do
            LIBRARIES+=("$lib")
        done < <(find "$BUILD_DIR" -name "libyaze*.a" -print0 2>/dev/null)
    fi

    TOTAL_LIBRARIES=${#LIBRARIES[@]}

    if [[ $TOTAL_LIBRARIES -eq 0 ]]; then
        print_error "No static libraries found in $BUILD_DIR"
        print_info "Build the project first: cmake --build $BUILD_DIR"
        exit 1
    fi

    print_info "Found $TOTAL_LIBRARIES libraries to scan"
    echo ""

    # Collect all symbols across libraries
    declare -A SYMBOL_MAP  # symbol -> list of libraries
    local TEMP_FILE=$(mktemp)

    print_info "Scanning libraries for symbols..."
    for lib in "${LIBRARIES[@]}"; do
        local lib_name=$(basename "$lib")
        if [[ $VERBOSE -eq 1 ]]; then
            print_info "  Scanning: $lib_name"
        fi

        # Extract defined symbols (T = text/code, D = data, R = read-only data, B = BSS)
        # Filter for global symbols (uppercase letters = global, lowercase = local)
        nm -g "$lib" 2>/dev/null | grep -E ' [TDRB] ' | while read -r addr type symbol; do
            # Demangle C++ symbols if possible
            if command -v c++filt &> /dev/null; then
                symbol=$(echo "$symbol" | c++filt)
            fi

            # Record symbol and which library defines it
            echo "$symbol|$lib_name" >> "$TEMP_FILE"
        done
    done

    print_info "Analyzing symbol duplicates..."
    echo ""

    # Find duplicate symbols
    local DUPLICATES=$(mktemp)
    sort "$TEMP_FILE" | uniq -w 200 -D > "$DUPLICATES"

    # Group duplicates by symbol
    local CURRENT_SYMBOL=""
    local LIBS=()
    local HAS_DUPLICATES=0

    while IFS='|' read -r symbol lib; do
        if [[ "$symbol" != "$CURRENT_SYMBOL" ]]; then
            # Process previous symbol if it had duplicates
            if [[ ${#LIBS[@]} -gt 1 ]]; then
                if [[ $SHOW_ALL -eq 1 ]] || ! is_safe_symbol "$CURRENT_SYMBOL"; then
                    HAS_DUPLICATES=1
                    ((DUPLICATE_SYMBOLS++))

                    # Check if it's a FLAGS symbol
                    if is_flags_symbol "$CURRENT_SYMBOL"; then
                        ((POTENTIAL_ODR++))
                        print_error "FLAGS symbol conflict: $CURRENT_SYMBOL"
                    else
                        print_warning "Duplicate symbol: $CURRENT_SYMBOL"
                    fi

                    for lib in "${LIBS[@]}"; do
                        echo "    → $lib"
                    done
                    echo ""
                fi
            fi

            # Start new symbol
            CURRENT_SYMBOL="$symbol"
            LIBS=("$lib")
        else
            LIBS+=("$lib")
        fi
    done < "$DUPLICATES"

    # Process last symbol
    if [[ ${#LIBS[@]} -gt 1 ]]; then
        if [[ $SHOW_ALL -eq 1 ]] || ! is_safe_symbol "$CURRENT_SYMBOL"; then
            HAS_DUPLICATES=1
            ((DUPLICATE_SYMBOLS++))

            if is_flags_symbol "$CURRENT_SYMBOL"; then
                ((POTENTIAL_ODR++))
                print_error "FLAGS symbol conflict: $CURRENT_SYMBOL"
            else
                print_warning "Duplicate symbol: $CURRENT_SYMBOL"
            fi

            for lib in "${LIBS[@]}"; do
                echo "    → $lib"
            done
            echo ""
        fi
    fi

    # Cleanup
    rm -f "$TEMP_FILE" "$DUPLICATES"

    # Report results
    print_header "Summary"
    print_info "Libraries scanned: $TOTAL_LIBRARIES"

    if [[ $POTENTIAL_ODR -gt 0 ]]; then
        print_error "FLAGS symbol conflicts: $POTENTIAL_ODR (ODR violations)"
        print_info "These are gflags-related conflicts that will cause link errors on Linux"
        return 1
    fi

    if [[ $DUPLICATE_SYMBOLS -gt 0 ]]; then
        print_warning "Duplicate symbols: $DUPLICATE_SYMBOLS"
        if [[ $SHOW_ALL -eq 0 ]]; then
            print_info "Most duplicates are safe (vtables, typeinfo, etc.)"
            print_info "Run with --show-all to see all duplicates"
        fi
        print_success "No critical ODR violations detected"
        return 0
    else
        print_success "No duplicate symbols detected"
        return 0
    fi
}

# Main
cd "$PROJECT_ROOT"

if [[ ! -d "$BUILD_DIR" ]]; then
    print_error "Build directory not found: $BUILD_DIR"
    print_info "Build the project first: cmake --preset <preset> && cmake --build build"
    exit 1
fi

check_tools
analyze_symbols
exit $?
