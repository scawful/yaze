#!/bin/bash
# Include Path Checker
# Validates that all required include paths are present in compile commands
#
# Usage:
#   ./scripts/check-include-paths.sh [build_directory]
#
# Exit codes:
#   0 - All checks passed
#   1 - Validation failed (missing includes detected)

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Determine build directory
BUILD_DIR="${1:-build}"

if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${RED}✗ Build directory not found: $BUILD_DIR${NC}"
    echo "Run cmake configure first: cmake --preset <preset-name>"
    exit 1
fi

if [ ! -f "$BUILD_DIR/compile_commands.json" ]; then
    echo -e "${RED}✗ compile_commands.json not found in $BUILD_DIR${NC}"
    echo "Make sure CMAKE_EXPORT_COMPILE_COMMANDS is ON"
    exit 1
fi

echo -e "${BLUE}=== Include Path Validation ===${NC}"
echo "Build directory: $BUILD_DIR"
echo ""

# Parse compile_commands.json using jq if available, otherwise use grep
if command -v jq &> /dev/null; then
    USE_JQ=true
    echo -e "${GREEN}✓ Using jq for JSON parsing${NC}"
else
    USE_JQ=false
    echo -e "${YELLOW}⚠ jq not found - using basic parsing (install jq for better results)${NC}"
fi

# Counter for issues
ERROR_COUNT=0
WARNING_COUNT=0
CHECK_COUNT=0

# Function to check if a path exists in compile commands
check_include_path() {
    local path="$1"
    local description="$2"
    local is_required="${3:-true}"

    CHECK_COUNT=$((CHECK_COUNT + 1))

    if [ "$USE_JQ" = true ]; then
        # Use jq to search compile commands
        if jq -e "[.[].command | select(contains(\"$path\"))] | length > 0" "$BUILD_DIR/compile_commands.json" &> /dev/null; then
            echo -e "${GREEN}✓${NC} $description: $path"
            return 0
        else
            if [ "$is_required" = true ]; then
                echo -e "${RED}✗${NC} Missing required include: $description"
                echo "   Expected path: $path"
                ERROR_COUNT=$((ERROR_COUNT + 1))
                return 1
            else
                echo -e "${YELLOW}⚠${NC} Optional include not found: $description"
                WARNING_COUNT=$((WARNING_COUNT + 1))
                return 0
            fi
        fi
    else
        # Basic grep-based search
        if grep -q "$path" "$BUILD_DIR/compile_commands.json"; then
            echo -e "${GREEN}✓${NC} $description: found"
            return 0
        else
            if [ "$is_required" = true ]; then
                echo -e "${RED}✗${NC} Missing required include: $description"
                ERROR_COUNT=$((ERROR_COUNT + 1))
                return 1
            else
                echo -e "${YELLOW}⚠${NC} Optional include not found: $description"
                WARNING_COUNT=$((WARNING_COUNT + 1))
                return 0
            fi
        fi
    fi
}

# Function to extract unique include directories from compile commands
extract_include_dirs() {
    echo -e "\n${BLUE}=== Include Directories Found ===${NC}"

    if [ "$USE_JQ" = true ]; then
        jq -r '.[].command' "$BUILD_DIR/compile_commands.json" | \
        grep -oE -- '-I[^ ]+' | \
        sed 's/^-I//' | \
        sort -u | \
        head -50
    else
        grep -oE -- '-I[^ ]+' "$BUILD_DIR/compile_commands.json" | \
        sed 's/^-I//' | \
        sort -u | \
        head -50
    fi
}

# Function to check for Abseil includes (Windows issue)
check_abseil_includes() {
    echo -e "\n${BLUE}=== Checking Abseil Includes (Windows Issue) ===${NC}"

    # Check if gRPC is enabled
    if [ -d "$BUILD_DIR/_deps/grpc-build" ]; then
        echo "gRPC build detected - checking Abseil paths..."

        # Check for the problematic missing include
        if [ -d "$BUILD_DIR/_deps/grpc-build/third_party/abseil-cpp" ]; then
            local absl_dir="$BUILD_DIR/_deps/grpc-build/third_party/abseil-cpp"
            check_include_path "$absl_dir" "Abseil from gRPC build" true
        fi

        # Check for generator expression variants
        if grep -q '\$<BUILD_INTERFACE:.*abseil-cpp' "$BUILD_DIR/compile_commands.json" 2>/dev/null; then
            echo -e "${YELLOW}⚠${NC} Generator expressions found in compile commands (may not be expanded)"
            WARNING_COUNT=$((WARNING_COUNT + 1))
        fi
    else
        echo -e "${YELLOW}⚠${NC} gRPC build not detected - skipping Abseil checks"
    fi
}

# Function to check platform-specific includes
check_platform_includes() {
    echo -e "\n${BLUE}=== Platform-Specific Includes ===${NC}"

    # Detect platform
    case "$(uname -s)" in
        Darwin*)
            echo "Platform: macOS"
            # macOS-specific checks
            check_include_path "SDL2" "SDL2 framework/library" true
            ;;
        Linux*)
            echo "Platform: Linux"
            # Linux-specific checks
            check_include_path "SDL2" "SDL2 library" true
            ;;
        MINGW*|MSYS*|CYGWIN*)
            echo "Platform: Windows"
            # Windows-specific checks
            check_include_path "SDL2" "SDL2 library" true
            ;;
        *)
            echo "Platform: Unknown"
            ;;
    esac
}

# Function to validate common dependencies
check_common_dependencies() {
    echo -e "\n${BLUE}=== Common Dependencies ===${NC}"

    # SDL2
    check_include_path "SDL" "SDL2 includes" true

    # ImGui (should be in build/_deps or ext/)
    if grep -q "imgui" "$BUILD_DIR/compile_commands.json"; then
        echo -e "${GREEN}✓${NC} ImGui includes found"
    else
        echo -e "${RED}✗${NC} ImGui includes not found"
        ERROR_COUNT=$((ERROR_COUNT + 1))
    fi

    # yaml-cpp
    if grep -q "yaml-cpp" "$BUILD_DIR/compile_commands.json"; then
        echo -e "${GREEN}✓${NC} yaml-cpp includes found"
    else
        echo -e "${YELLOW}⚠${NC} yaml-cpp includes not found (may be optional)"
        WARNING_COUNT=$((WARNING_COUNT + 1))
    fi
}

# Function to check for suspicious configurations
check_suspicious_configs() {
    echo -e "\n${BLUE}=== Suspicious Configurations ===${NC}"

    # Check for missing -I flags entirely
    if [ "$USE_JQ" = true ]; then
        local compile_cmds=$(jq -r '.[].command' "$BUILD_DIR/compile_commands.json" | wc -l)
        local include_cmds=$(jq -r '.[].command' "$BUILD_DIR/compile_commands.json" | grep -c -- '-I' || true)

        if [ "$include_cmds" -eq 0 ] && [ "$compile_cmds" -gt 0 ]; then
            echo -e "${RED}✗${NC} No -I flags found in any compile command!"
            ERROR_COUNT=$((ERROR_COUNT + 1))
        else
            echo -e "${GREEN}✓${NC} Include flags present ($include_cmds/$compile_cmds commands)"
        fi
    fi

    # Check for absolute vs relative paths
    if grep -q -- '-I\.\.' "$BUILD_DIR/compile_commands.json" 2>/dev/null; then
        echo -e "${YELLOW}⚠${NC} Relative include paths found (../) - may cause issues"
        WARNING_COUNT=$((WARNING_COUNT + 1))
    fi

    # Check for duplicate include paths
    local duplicates
    if [ "$USE_JQ" = true ]; then
        duplicates=$(jq -r '.[].command' "$BUILD_DIR/compile_commands.json" | \
                    grep -oE -- '-I[^ ]+' | \
                    sort | uniq -d | wc -l)
        if [ "$duplicates" -gt 0 ]; then
            echo -e "${YELLOW}⚠${NC} $duplicates duplicate include paths found (usually harmless)"
        else
            echo -e "${GREEN}✓${NC} No duplicate include paths"
        fi
    fi
}

# Function to analyze a specific source file
analyze_file_includes() {
    local source_file="${1:-}"

    if [ -z "$source_file" ]; then
        return
    fi

    echo -e "\n${BLUE}=== Analyzing: $source_file ===${NC}"

    if [ "$USE_JQ" = true ]; then
        local includes=$(jq -r ".[] | select(.file | contains(\"$source_file\")) | .command" \
                        "$BUILD_DIR/compile_commands.json" | \
                        grep -oE -- '-I[^ ]+' | \
                        sed 's/^-I//')

        if [ -n "$includes" ]; then
            echo "Include paths for this file:"
            echo "$includes" | while read -r path; do
                echo "  - $path"
            done
        else
            echo -e "${YELLOW}⚠${NC} File not found in compile commands"
        fi
    fi
}

# Main execution
echo -e "${BLUE}=== Running Include Path Checks ===${NC}\n"

check_common_dependencies
check_platform_includes
check_abseil_includes
check_suspicious_configs

# Optional: Show all include directories
if [ "${VERBOSE:-0}" -eq 1 ]; then
    extract_include_dirs
fi

# Summary
echo -e "\n${BLUE}=== Summary ===${NC}"
echo "Checks performed: $CHECK_COUNT"

if [ $ERROR_COUNT -gt 0 ]; then
    echo -e "${RED}Errors: $ERROR_COUNT${NC}"
fi

if [ $WARNING_COUNT -gt 0 ]; then
    echo -e "${YELLOW}Warnings: $WARNING_COUNT${NC}"
fi

if [ $ERROR_COUNT -eq 0 ] && [ $WARNING_COUNT -eq 0 ]; then
    echo -e "${GREEN}✓ All include path checks passed!${NC}"
    exit 0
elif [ $ERROR_COUNT -eq 0 ]; then
    echo -e "${YELLOW}⚠ Include paths have warnings but should work${NC}"
    exit 0
else
    echo -e "${RED}✗ Include path validation failed - fix errors before building${NC}"
    echo ""
    echo "Common fixes:"
    echo "  1. Reconfigure: cmake --preset <preset> --fresh"
    echo "  2. Check dependencies.cmake for missing includes"
    echo "  3. On Windows with gRPC: verify Abseil include propagation"
    exit 1
fi
