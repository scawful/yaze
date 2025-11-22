#!/bin/bash
# Validate GitHub Actions composite action structure

set -e

echo "Validating GitHub Actions composite actions..."

ACTIONS_DIR=".github/actions"
REQUIRED_FIELDS=("name" "description" "runs")

validate_action() {
    local action_file="$1"
    local action_name=$(basename $(dirname "$action_file"))
    
    echo "Checking $action_name..."
    
    # Check if file exists
    if [ ! -f "$action_file" ]; then
        echo "  ✗ action.yml not found"
        return 1
    fi
    
    # Check required fields
    for field in "${REQUIRED_FIELDS[@]}"; do
        if ! grep -q "^${field}:" "$action_file"; then
            echo "  ✗ Missing required field: $field"
            return 1
        fi
    done
    
    # Check for 'using: composite'
    if ! grep -q "using: 'composite'" "$action_file"; then
        echo "  ✗ Not marked as composite action"
        return 1
    fi
    
    echo "  ✓ Valid composite action"
    return 0
}

# Validate all actions
all_valid=true
for action_yml in "$ACTIONS_DIR"/*/action.yml; do
    if ! validate_action "$action_yml"; then
        all_valid=false
    fi
done

# Check that CI workflow references actions correctly
echo ""
echo "Checking CI workflow..."
CI_FILE=".github/workflows/ci.yml"

if [ ! -f "$CI_FILE" ]; then
    echo "  ✗ CI workflow not found"
    all_valid=false
else
    # Check for checkout before action usage
    if grep -q "uses: actions/checkout@v4" "$CI_FILE"; then
        echo "  ✓ Repository checkout step present"
    else
        echo "  ✗ Missing checkout step"
        all_valid=false
    fi
    
    # Check for composite action references
    action_refs=$(grep -c "uses: ./.github/actions/" "$CI_FILE" || echo "0")
    if [ "$action_refs" -gt 0 ]; then
        echo "  ✓ Found $action_refs composite action references"
    else
        echo "  ✗ No composite action references found"
        all_valid=false
    fi
fi

echo ""
if [ "$all_valid" = true ]; then
    echo "✓ All validations passed!"
    exit 0
else
    echo "✗ Some validations failed"
    exit 1
fi

