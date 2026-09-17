#!/usr/bin/env bash
#
# Changed-file linting fast path for yaze.
# Wraps clang-format and clang-tidy with project-specific configuration.
#
# Usage:
#   scripts/lint.sh [check|fix] [files...]
#
#   check (default) - Check for issues without modifying files
#   fix             - Automatically fix formatting and some tidy issues
#   files...        - Optional list of files to process (defaults to all source files)
#
# clang-tidy needs a compile database. The canonical one is the repo-root
# compile_commands.json maintained by scripts/dev/update_compile_commands.sh.
# scripts/quality_check.sh is the slower whole-repository pass.

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
# shellcheck source=scripts/lib/clang_tools.sh
source "${SCRIPT_DIR}/lib/clang_tools.sh"
cd "$PROJECT_ROOT"

MODE="check"
if [[ "$1" == "fix" ]]; then
    MODE="fix"
    shift
elif [[ "$1" == "check" ]]; then
    shift
fi

# Files to process
FILES="$*"
if [[ -z "$FILES" ]]; then
    # Find all source files, excluding third-party libraries
    # Using git ls-files if available to respect .gitignore
    if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
        FILES=$(git ls-files 'src/*.cc' 'src/*.h' 'test/*.cc' 'test/*.h' | grep -v "src/lib/")
    else
        FILES=$(find src test -name "*.cc" -o -name "*.h" | grep -v "src/lib/")
    fi
fi

CLANG_FORMAT=$(yaze_find_clang_format || true)
CLANG_TIDY=$(yaze_find_clang_tidy || true)

if [[ -z "$CLANG_FORMAT" ]]; then
    echo -e "${RED}Error: clang-format not found.${NC}"
    echo -e "Install major $(yaze_clang_pinned_major) to match CI (see .clang-format-version)."
    exit 1
fi

if [[ -z "$CLANG_TIDY" ]]; then
    echo -e "${YELLOW}Warning: clang-tidy not found. Skipping tidy checks.${NC}"
fi

yaze_warn_clang_version "$CLANG_FORMAT" clang-format

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
    
    # The repo-root compile_commands.json is canonical (see .clangd); build/ is
    # only a fallback for older local layouts.
    BUILD_PATH=""
    if [[ -f "compile_commands.json" ]]; then
        BUILD_PATH="."
    elif [[ -f "build/compile_commands.json" ]]; then
        BUILD_PATH="build"
    else
        echo -e "${YELLOW}compile_commands.json not found.${NC}"
        echo -e "Generate it with: ${YELLOW}cmake --preset mac-ai && scripts/dev/update_compile_commands.sh mac-ai${NC}"
    fi

    if [[ -n "$BUILD_PATH" ]]; then
        TIDY_ARGS="-p $BUILD_PATH --quiet"
        [[ "$MODE" == "fix" ]] && TIDY_ARGS="$TIDY_ARGS --fix"
        
        echo "$FILES" | xargs "$CLANG_TIDY" $TIDY_ARGS


        echo -e "${GREEN}Clang-tidy finished.${NC}"
    else
        echo -e "${YELLOW}Skipping clang-tidy (compile_commands.json not found).${NC}"
    fi
fi

echo -e "\n${GREEN}Linting complete.${NC}"

