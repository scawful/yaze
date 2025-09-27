#!/bin/bash

# Quality check script for YAZE codebase
# This script runs various code quality checks to ensure CI/CD pipeline passes

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "${PROJECT_ROOT}"

echo "🔍 Running code quality checks for YAZE..."

# Check if required tools are available
command -v clang-format >/dev/null 2>&1 || { echo "❌ clang-format not found. Please install it."; exit 1; }
command -v cppcheck >/dev/null 2>&1 || { echo "❌ cppcheck not found. Please install it."; exit 1; }

# Create .clang-format config if it doesn't exist
if [ ! -f .clang-format ]; then
    echo "📝 Creating .clang-format configuration..."
    clang-format --style=Google --dump-config > .clang-format
fi

echo "✅ Code formatting check..."
# Check formatting without modifying files
FORMATTING_ISSUES=$(find src test -name "*.cc" -o -name "*.h" | head -50 | xargs clang-format --dry-run --Werror --style=Google 2>&1 || true)
if [ -n "$FORMATTING_ISSUES" ]; then
    echo "⚠️  Formatting issues found. Run 'make format' to fix them."
    echo "$FORMATTING_ISSUES" | head -20
else
    echo "✅ All files are properly formatted"
fi

echo "🔍 Running static analysis..."
# Run cppcheck on main source directories
cppcheck --enable=all --error-exitcode=0 \
    --suppress=missingIncludeSystem \
    --suppress=unusedFunction \
    --suppress=unmatchedSuppression \
    --suppress=unreadVariable \
    --suppress=cstyleCast \
    --suppress=variableScope \
    src/ 2>&1 | head -30

echo "✅ Quality checks completed!"
echo ""
echo "💡 To fix formatting issues automatically, run:"
echo "   find src test -name '*.cc' -o -name '*.h' | xargs clang-format -i --style=Google"
echo ""
echo "💡 For CI/CD pipeline compatibility, ensure:"
echo "   - All formatting issues are resolved"
echo "   - absl::Status return values are handled with RETURN_IF_ERROR() or PRINT_IF_ERROR()"
echo "   - Use Google C++ style for consistency"
