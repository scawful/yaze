#!/usr/bin/env bash
# Git hooks installer for yaze
# Installs pre-push hook to run validation before pushing
#
# Usage:
#   scripts/install-git-hooks.sh [install|uninstall|status]
#
# Commands:
#   install    - Install pre-push hook (default)
#   uninstall  - Remove pre-push hook
#   status     - Show hook installation status

set -euo pipefail

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

REPO_ROOT=$(git rev-parse --show-toplevel 2>/dev/null || pwd)
HOOK_DIR="$REPO_ROOT/.git/hooks"
HOOK_FILE="$HOOK_DIR/pre-push"
HOOK_SCRIPT="$REPO_ROOT/scripts/pre-push.sh"

print_success() {
  echo -e "${GREEN}✅ $1${NC}"
}

print_error() {
  echo -e "${RED}❌ $1${NC}"
}

print_warning() {
  echo -e "${YELLOW}⚠️  $1${NC}"
}

print_info() {
  echo -e "${BLUE}ℹ️  $1${NC}"
}

# Check if we're in a git repository
check_git_repo() {
  if ! git rev-parse --git-dir > /dev/null 2>&1; then
    print_error "Not in a git repository"
    exit 1
  fi
}

# Check if hook directory exists
check_hook_dir() {
  if [ ! -d "$HOOK_DIR" ]; then
    print_error "Git hooks directory not found: $HOOK_DIR"
    exit 1
  fi
}

# Check if pre-push script exists
check_prepush_script() {
  if [ ! -f "$HOOK_SCRIPT" ]; then
    print_error "Pre-push script not found: $HOOK_SCRIPT"
    print_info "Make sure you're running this from the repository root"
    exit 1
  fi
}

# Show hook status
show_status() {
  echo -e "${BLUE}=== Git Hook Status ===${NC}\n"

  if [ -f "$HOOK_FILE" ]; then
    print_success "Pre-push hook is installed"
    echo ""
    echo "Hook location: $HOOK_FILE"
    echo ""

    # Check if it's our hook
    if grep -q "scripts/pre-push.sh" "$HOOK_FILE" 2>/dev/null; then
      print_info "Hook type: yaze validation hook"
    else
      print_warning "Hook type: Custom/unknown (not yaze default)"
      print_info "To reinstall yaze hook, run: scripts/install-git-hooks.sh install"
    fi
  else
    print_warning "Pre-push hook is NOT installed"
    echo ""
    print_info "To install, run: scripts/install-git-hooks.sh install"
  fi

  echo ""
  echo "Pre-push script: $HOOK_SCRIPT"
  if [ -x "$HOOK_SCRIPT" ]; then
    print_success "Script is executable"
  else
    print_warning "Script is not executable"
    print_info "Run: chmod +x $HOOK_SCRIPT"
  fi
}

# Install hook
install_hook() {
  echo -e "${BLUE}=== Installing Pre-Push Hook ===${NC}\n"

  # Backup existing hook if present
  if [ -f "$HOOK_FILE" ]; then
    print_warning "Existing hook found"

    # Check if it's our hook
    if grep -q "scripts/pre-push.sh" "$HOOK_FILE" 2>/dev/null; then
      print_info "Existing hook is already yaze validation hook"
      print_info "Updating hook..."
    else
      local backup="$HOOK_FILE.backup.$(date +%Y%m%d_%H%M%S)"
      print_info "Backing up to: $backup"
      cp "$HOOK_FILE" "$backup"
    fi
  fi

  # Create hook
  cat > "$HOOK_FILE" << 'EOF'
#!/usr/bin/env bash
# Pre-push hook for yaze
# Automatically installed by scripts/install-git-hooks.sh
#
# To bypass this hook, use: git push --no-verify

# Get repository root
REPO_ROOT=$(git rev-parse --show-toplevel)

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}Running pre-push validation...${NC}\n"

# Run validation script
if ! "$REPO_ROOT/scripts/pre-push.sh"; then
  echo ""
  echo -e "${RED}Pre-push validation failed!${NC}"
  echo ""
  echo "Fix the issues above and try again."
  echo "To bypass this check (not recommended), use: git push --no-verify"
  exit 1
fi

echo ""
echo -e "${GREEN}Pre-push validation passed!${NC}"
exit 0
EOF

  # Make hook executable
  chmod +x "$HOOK_FILE"

  print_success "Pre-push hook installed successfully"
  echo ""
  print_info "Hook location: $HOOK_FILE"
  print_info "The hook will run automatically before each push"
  print_info "To bypass: git push --no-verify (use sparingly)"
  echo ""
  print_info "Test the hook with: scripts/pre-push.sh"
}

# Uninstall hook
uninstall_hook() {
  echo -e "${BLUE}=== Uninstalling Pre-Push Hook ===${NC}\n"

  if [ ! -f "$HOOK_FILE" ]; then
    print_warning "No hook to uninstall"
    exit 0
  fi

  # Check if it's our hook before removing
  if grep -q "scripts/pre-push.sh" "$HOOK_FILE" 2>/dev/null; then
    rm "$HOOK_FILE"
    print_success "Pre-push hook uninstalled"
  else
    print_warning "Hook exists but doesn't appear to be yaze validation hook"
    print_info "Manual removal required: rm $HOOK_FILE"
    exit 1
  fi
}

# Main
main() {
  local command="${1:-install}"

  check_git_repo
  check_hook_dir

  case "$command" in
    install)
      check_prepush_script
      install_hook
      ;;
    uninstall)
      uninstall_hook
      ;;
    status)
      show_status
      ;;
    --help|-h|help)
      grep '^#' "$0" | sed 's/^# \?//'
      exit 0
      ;;
    *)
      print_error "Unknown command: $command"
      echo "Use: install, uninstall, status, or --help"
      exit 1
      ;;
  esac
}

main "$@"
