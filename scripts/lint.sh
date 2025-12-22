#!/usr/bin/env bash
#
# Unified linting script for yaze
# Wraps clang-format and clang-tidy with project-specific configuration
#
# Usage:
#   scripts/lint.sh [check|fix] [files...]
#
#   check (default) - Check for issues without modifying files
#   fix             - Automatically fix formatting and some tidy issues
#   files...        - Optional list of files to process (defaults to all source files)

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Configuration
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

MODE="check"
if [[ "$1" == "fix" ]]; then
    MODE="fix"
    shift
elif [[ "$1" == "check" ]]; then
    shift
fi

# Files to process
FILES="$@"
if [[ -z "$FILES" ]]; then
    # Find all source files, excluding third-party libraries
    # Using git ls-files if available to respect .gitignore
    if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
        FILES=$(git ls-files 'src/*.cc' 'src/*.h' 'test/*.cc' 'test/*.h' | grep -v "src/lib/")
    else
        FILES=$(find src test -name "*.cc" -o -name "*.h" | grep -v "src/lib/")
    fi
fi

# Find tools
find_tool() {
    local names=("$@")
    for name in "${names[@]}"; do
        if command -v "$name" >/dev/null 2>&1; then
            echo "$name"
            return 0
        fi
    done
    
    # Check Homebrew LLVM paths on macOS
    if [[ "$(uname)" == "Darwin" ]]; then
        local brew_prefix
        if command -v brew >/dev/null 2>&1; then
            brew_prefix=$(brew --prefix llvm 2>/dev/null)
            if [[ -n "$brew_prefix" ]]; then
                for name in "${names[@]}"; do
                    if [[ -x "$brew_prefix/bin/$name" ]]; then
                        echo "$brew_prefix/bin/$name"
                        return 0
                    fi
                done
            fi
        fi
    fi
    return 1
}

CLANG_FORMAT=$(find_tool clang-format-18 clang-format-17 clang-format)
CLANG_TIDY=$(find_tool clang-tidy-18 clang-tidy-17 clang-tidy)

if [[ -z "$CLANG_FORMAT" ]]; then
    echo -e "${RED}Error: clang-format not found.${NC}"
    exit 1
fi

if [[ -z "$CLANG_TIDY" ]]; then
    echo -e "${YELLOW}Warning: clang-tidy not found. Skipping tidy checks.${NC}"
fi

echo -e "${BLUE}Using clang-format: $CLANG_FORMAT${NC}"
[[ -n "$CLANG_TIDY" ]] && echo -e "${BLUE}Using clang-tidy: $CLANG_TIDY${NC}"

# Run clang-format
echo -e "\n${BLUE}=== Running clang-format ===${NC}"
if [[ "$MODE" == "fix" ]]; then
    echo "$FILES" | xargs "$CLANG_FORMAT" -i --style=file
    echo -e "${GREEN}Formatting applied.${NC}"
else
    # --dry-run --Werror returns 0 if clean, non-zero if changes needed (or error)
    # Actually --dry-run prints replacements, --Werror returns error on warnings (formatting violations are not warnings by default)
    # To check if formatted: use --dry-run --Werror with output check or just check exit code if it supports it.
    # Standard way: clang-format --dry-run --Werror <file>
    
    if echo "$FILES" | xargs "$CLANG_FORMAT" --dry-run --Werror --style=file 2>&1; then
        echo -e "${GREEN}Format check passed.${NC}"
    else
        echo -e "${RED}Format check failed.${NC}"
        echo -e "Run '${YELLOW}scripts/lint.sh fix${NC}' to apply formatting."
        exit 1
    fi
fi

# Run clang-tidy
if [[ -n "$CLANG_TIDY" ]]; then
    echo -e "\n${BLUE}=== Running clang-tidy ===${NC}"
    
    # Build compile_commands.json if missing (needed for clang-tidy)
    if [[ ! -f "build/compile_commands.json" && ! -f "compile_commands.json" ]]; then
         echo -e "${YELLOW}compile_commands.json not found. Attempting to generate...${NC}"
         if command -v cmake >/dev/null; then
             cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON >/dev/null
         else
             echo -e "${RED}cmake not found. Cannot generate compile_commands.json.${NC}"
         fi
    fi

    # Find compile_commands.json
    BUILD_PATH=""
    if [[ -f "build/compile_commands.json" ]]; then
        BUILD_PATH="build"
    elif [[ -f "compile_commands.json" ]]; then
        BUILD_PATH="."
    fi

    if [[ -n "$BUILD_PATH" ]]; then
        TIDY_ARGS="-p $BUILD_PATH --quiet"
        [[ "$MODE" == "fix" ]] && TIDY_ARGS="$TIDY_ARGS --fix"
        
        # Use parallel if available
        if command -v parallel >/dev/null 2>&1; then
            # parallel processing would require a different invocation
            # For now, just run simple xargs
            echo "$FILES" | xargs "$CLANG_TIDY" $TIDY_ARGS
        else
            echo "$FILES" | xargs "$CLANG_TIDY" $TIDY_ARGS
        fi
        
        echo -e "${GREEN}Clang-tidy finished.${NC}"
    else
        echo -e "${YELLOW}Skipping clang-tidy (compile_commands.json not found).${NC}"
    fi
fi

echo -e "\n${GREEN}Linting complete.${NC}"

