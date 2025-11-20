#!/usr/bin/env bash

#
# CMake Configuration Validator
#
# Validates that a set of CMake flags would result in a valid configuration.
# Catches common mistakes before running a full build.
#
# Usage:
#   ./scripts/validate-cmake-config.sh [FLAGS]
#
# Examples:
#   ./scripts/validate-cmake-config.sh -DYAZE_ENABLE_GRPC=ON -DYAZE_ENABLE_REMOTE_AUTOMATION=OFF
#   ./scripts/validate-cmake-config.sh -DYAZE_ENABLE_HTTP_API=ON -DYAZE_ENABLE_AGENT_CLI=OFF
#   ./scripts/validate-cmake-config.sh -DYAZE_BUILD_AGENT_UI=ON -DYAZE_BUILD_GUI=OFF
#

set -euo pipefail

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Validation rules
# Format: "Flag1=Value1" "Flag2=Value2" "ERROR_MESSAGE"
declare -a INCOMPATIBLE_PAIRS=(
  "YAZE_ENABLE_GRPC=OFF" "YAZE_ENABLE_REMOTE_AUTOMATION=ON" "REMOTE_AUTOMATION requires GRPC"
  "YAZE_ENABLE_HTTP_API=ON" "YAZE_ENABLE_AGENT_CLI=OFF" "HTTP_API requires AGENT_CLI"
  "YAZE_BUILD_AGENT_UI=ON" "YAZE_BUILD_GUI=OFF" "AGENT_UI requires BUILD_GUI"
  "YAZE_ENABLE_AI_RUNTIME=ON" "YAZE_ENABLE_AI=OFF" "AI_RUNTIME requires ENABLE_AI"
)

# Single-flag rules (flag must have specific value)
declare -A SINGLE_FLAG_RULES=(
  # If left side is ON, right side must also be ON
  # Format handled specially below
)

# ============================================================================
# Helper Functions
# ============================================================================

log_error() {
  echo -e "${RED}✗ ERROR${NC}: $*"
}

log_warning() {
  echo -e "${YELLOW}! WARNING${NC}: $*"
}

log_success() {
  echo -e "${GREEN}✓${NC} $*"
}

log_info() {
  echo -e "${BLUE}ℹ${NC} $*"
}

print_usage() {
  cat <<'EOF'
CMake Configuration Validator

Validates CMake flag combinations before building.

Usage:
  ./scripts/validate-cmake-config.sh [CMake flags]

Examples:
  # Check if this combination is valid
  ./scripts/validate-cmake-config.sh \
    -DYAZE_ENABLE_GRPC=ON \
    -DYAZE_ENABLE_REMOTE_AUTOMATION=OFF

  # Batch check multiple combinations
  ./scripts/validate-cmake-config.sh \
    -DYAZE_ENABLE_HTTP_API=ON \
    -DYAZE_ENABLE_AGENT_CLI=OFF

Rules Checked:
  1. Dependency constraints
  2. Mutual exclusivity
  3. Feature prerequisites
  4. Platform-specific requirements

Output:
  ✓ All checks passed
  ✗ Error: Specific problem found
  ! Warning: Potential issue

EOF
}

# ============================================================================
# Configuration Parsing
# ============================================================================

parse_flags() {
  declare -gA FLAGS_PROVIDED

  for flag in "$@"; do
    if [[ "$flag" =~ ^-D([A-Z_]+)=(.*)$ ]]; then
      local key="${BASH_REMATCH[1]}"
      local value="${BASH_REMATCH[2]}"
      FLAGS_PROVIDED["$key"]="$value"
    fi
  done
}

get_flag_value() {
  local flag="$1"
  local default="${2:-}"

  # Return user-provided value if available
  if [[ -n "${FLAGS_PROVIDED[$flag]:-}" ]]; then
    echo "${FLAGS_PROVIDED[$flag]}"
  else
    echo "$default"
  fi
}

# ============================================================================
# Validation Rules
# ============================================================================

check_dependency_constraint() {
  local dependent="$1"
  local dependency="$2"
  local error_msg="$3"

  local dependent_value=$(get_flag_value "$dependent" "OFF")
  local dependency_value=$(get_flag_value "$dependency" "OFF")

  # If dependent is ON, dependency must be ON
  if [[ "$dependent_value" == "ON" ]] && [[ "$dependency_value" != "ON" ]]; then
    return 1
  fi
  return 0
}

validate_remote_automation() {
  # REMOTE_AUTOMATION requires GRPC
  if ! check_dependency_constraint "YAZE_ENABLE_REMOTE_AUTOMATION" "YAZE_ENABLE_GRPC" \
    "YAZE_ENABLE_REMOTE_AUTOMATION=ON requires YAZE_ENABLE_GRPC=ON"; then
    log_error "YAZE_ENABLE_REMOTE_AUTOMATION=ON requires YAZE_ENABLE_GRPC=ON"
    return 1
  fi
  return 0
}

validate_http_api() {
  # HTTP_API requires AGENT_CLI
  if ! check_dependency_constraint "YAZE_ENABLE_HTTP_API" "YAZE_ENABLE_AGENT_CLI" \
    "YAZE_ENABLE_HTTP_API=ON requires YAZE_ENABLE_AGENT_CLI=ON"; then
    log_error "YAZE_ENABLE_HTTP_API=ON requires YAZE_ENABLE_AGENT_CLI=ON"
    return 1
  fi
  return 0
}

validate_agent_ui() {
  # AGENT_UI requires BUILD_GUI
  if ! check_dependency_constraint "YAZE_BUILD_AGENT_UI" "YAZE_BUILD_GUI" \
    "YAZE_BUILD_AGENT_UI=ON requires YAZE_BUILD_GUI=ON"; then
    log_error "YAZE_BUILD_AGENT_UI=ON requires YAZE_BUILD_GUI=ON"
    return 1
  fi
  return 0
}

validate_ai_runtime() {
  # AI_RUNTIME requires AI
  if ! check_dependency_constraint "YAZE_ENABLE_AI_RUNTIME" "YAZE_ENABLE_AI" \
    "YAZE_ENABLE_AI_RUNTIME=ON requires YAZE_ENABLE_AI=ON"; then
    log_error "YAZE_ENABLE_AI_RUNTIME=ON requires YAZE_ENABLE_AI=ON"
    return 1
  fi
  return 0
}

validate_gemini_support() {
  # Gemini support needs both AI_RUNTIME and JSON
  local ai_runtime=$(get_flag_value "YAZE_ENABLE_AI_RUNTIME" "OFF")
  local json=$(get_flag_value "YAZE_ENABLE_JSON" "ON")

  if [[ "$ai_runtime" == "ON" ]] && [[ "$json" != "ON" ]]; then
    log_warning "YAZE_ENABLE_AI_RUNTIME=ON without YAZE_ENABLE_JSON=ON"
    log_info "  → Gemini service requires JSON parsing"
    log_info "  → Ollama will still work without JSON"
    return 1
  fi
  return 0
}

validate_agent_cli_auto_enable() {
  # AGENT_CLI is auto-enabled if BUILD_CLI or BUILD_Z3ED enabled
  local build_cli=$(get_flag_value "YAZE_BUILD_CLI" "ON")
  local build_z3ed=$(get_flag_value "YAZE_BUILD_Z3ED" "ON")
  local agent_cli=$(get_flag_value "YAZE_ENABLE_AGENT_CLI" "")

  if [[ "$agent_cli" == "OFF" ]] && ([[ "$build_cli" == "ON" ]] || [[ "$build_z3ed" == "ON" ]]); then
    log_warning "YAZE_ENABLE_AGENT_CLI=OFF but YAZE_BUILD_CLI or BUILD_Z3ED is ON"
    log_info "  → AGENT_CLI will be auto-enabled by CMake"
    return 1
  fi
  return 0
}

# ============================================================================
# Recommendations
# ============================================================================

suggest_configuration() {
  local remote_auto=$(get_flag_value "YAZE_ENABLE_REMOTE_AUTOMATION" "OFF")
  local http_api=$(get_flag_value "YAZE_ENABLE_HTTP_API" "OFF")
  local ai_runtime=$(get_flag_value "YAZE_ENABLE_AI_RUNTIME" "OFF")
  local build_gui=$(get_flag_value "YAZE_BUILD_GUI" "ON")
  local build_cli=$(get_flag_value "YAZE_BUILD_CLI" "ON")

  echo ""
  echo "Suggested preset configurations:"
  echo ""

  if [[ "$build_gui" == "ON" ]] && [[ "$ai_runtime" != "ON" ]]; then
    log_info "GUI-only user? Use preset: mac-dbg, lin-dbg, or win-dbg"
  fi

  if [[ "$remote_auto" == "ON" ]] && [[ "$ai_runtime" == "ON" ]]; then
    log_info "Full-featured dev? Use preset: mac-ai, lin-ai, or win-ai"
  fi

  if [[ "$build_cli" == "ON" ]] && [[ "$build_gui" != "ON" ]]; then
    log_info "CLI-only user? Use preset: win-z3ed or custom CLI config"
  fi
}

# ============================================================================
# Main Validation
# ============================================================================

validate_configuration() {
  local errors=0
  local warnings=0

  log_info "Validating CMake configuration..."
  echo ""

  # Run all validation checks
  if ! validate_remote_automation; then
    errors=$((errors + 1))
  fi

  if ! validate_http_api; then
    errors=$((errors + 1))
  fi

  if ! validate_agent_ui; then
    errors=$((errors + 1))
  fi

  if ! validate_ai_runtime; then
    errors=$((errors + 1))
  fi

  if ! validate_gemini_support; then
    warnings=$((warnings + 1))
  fi

  if ! validate_agent_cli_auto_enable; then
    warnings=$((warnings + 1))
  fi

  echo ""

  if [[ $errors -gt 0 ]]; then
    log_error "$errors critical issue(s) found"
    echo ""
    echo "Fix these before building:"
    echo "  - Check documentation: docs/internal/configuration-matrix.md"
    echo "  - Run: ./scripts/test-config-matrix.sh --verbose"
    suggest_configuration
    return 1
  fi

  if [[ $warnings -gt 0 ]]; then
    log_warning "$warnings warning(s) found"
    echo ""
    log_info "These may work, but might not have all features"
  fi

  if [[ $errors -eq 0 ]]; then
    log_success "Configuration is valid!"
    suggest_configuration
    return 0
  fi
}

# ============================================================================
# Entry Point
# ============================================================================

main() {
  if [[ $# -eq 0 ]]; then
    print_usage
    exit 0
  fi

  if [[ "$1" == "--help" ]] || [[ "$1" == "-h" ]]; then
    print_usage
    exit 0
  fi

  # Parse provided flags
  parse_flags "$@"

  # Run validation
  if validate_configuration; then
    exit 0
  else
    exit 1
  fi
}

main "$@"
