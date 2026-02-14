#!/usr/bin/env bash
# Git hooks installer for yaze.
# Installs both pre-commit and pre-push hooks by default.
#
# Usage:
#   scripts/install-git-hooks.sh [install|uninstall|status|install-pre-commit|install-pre-push|uninstall-pre-commit|uninstall-pre-push]

set -euo pipefail

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

REPO_ROOT="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
HOOK_DIR="$REPO_ROOT/.git/hooks"

PRECOMMIT_HOOK_FILE="$HOOK_DIR/pre-commit"
PREPUSH_HOOK_FILE="$HOOK_DIR/pre-push"
PRECOMMIT_SCRIPT="$REPO_ROOT/scripts/pre-commit.sh"
PREPUSH_SCRIPT="$REPO_ROOT/scripts/pre-push.sh"

print_success() { echo -e "${GREEN}[ok] $1${NC}"; }
print_error() { echo -e "${RED}[err] $1${NC}"; }
print_warning() { echo -e "${YELLOW}[warn] $1${NC}"; }
print_info() { echo -e "${BLUE}[info] $1${NC}"; }

check_git_repo() {
  if ! git rev-parse --git-dir >/dev/null 2>&1; then
    print_error "Not in a git repository"
    exit 1
  fi
}

check_hook_dir() {
  if [[ ! -d "$HOOK_DIR" ]]; then
    print_error "Git hooks directory not found: $HOOK_DIR"
    exit 1
  fi
}

backup_hook_if_needed() {
  local hook_file="$1"
  local marker="$2"

  if [[ -f "$hook_file" ]] && ! grep -q "$marker" "$hook_file" 2>/dev/null; then
    local backup="${hook_file}.backup.$(date +%Y%m%d_%H%M%S)"
    cp "$hook_file" "$backup"
    print_info "Backed up existing hook to $backup"
  fi
}

install_precommit_hook() {
  if [[ ! -x "$PRECOMMIT_SCRIPT" ]]; then
    if [[ -f "$PRECOMMIT_SCRIPT" ]]; then
      chmod +x "$PRECOMMIT_SCRIPT"
    else
      print_error "Missing script: $PRECOMMIT_SCRIPT"
      exit 1
    fi
  fi

  backup_hook_if_needed "$PRECOMMIT_HOOK_FILE" "scripts/pre-commit.sh"

  cat > "$PRECOMMIT_HOOK_FILE" <<'EOF'
#!/usr/bin/env bash
# Auto-installed by scripts/install-git-hooks.sh
# To bypass: git commit --no-verify

REPO_ROOT=$(git rev-parse --show-toplevel)

if ! "$REPO_ROOT/scripts/pre-commit.sh"; then
  echo ""
  echo "pre-commit checks failed"
  echo "Fix issues and re-commit, or bypass once with: git commit --no-verify"
  exit 1
fi

exit 0
EOF

  chmod +x "$PRECOMMIT_HOOK_FILE"
  print_success "Installed pre-commit hook"
}

install_prepush_hook() {
  if [[ ! -x "$PREPUSH_SCRIPT" ]]; then
    if [[ -f "$PREPUSH_SCRIPT" ]]; then
      chmod +x "$PREPUSH_SCRIPT"
    else
      print_error "Missing script: $PREPUSH_SCRIPT"
      exit 1
    fi
  fi

  backup_hook_if_needed "$PREPUSH_HOOK_FILE" "scripts/pre-push.sh"

  cat > "$PREPUSH_HOOK_FILE" <<'EOF'
#!/usr/bin/env bash
# Auto-installed by scripts/install-git-hooks.sh
# To bypass: git push --no-verify

REPO_ROOT=$(git rev-parse --show-toplevel)

if ! "$REPO_ROOT/scripts/pre-push.sh"; then
  echo ""
  echo "pre-push checks failed"
  echo "Fix issues and push again, or bypass once with: git push --no-verify"
  exit 1
fi

exit 0
EOF

  chmod +x "$PREPUSH_HOOK_FILE"
  print_success "Installed pre-push hook"
}

uninstall_hook_if_managed() {
  local hook_file="$1"
  local marker="$2"
  local name="$3"

  if [[ ! -f "$hook_file" ]]; then
    print_warning "$name hook not installed"
    return
  fi

  if grep -q "$marker" "$hook_file" 2>/dev/null; then
    rm -f "$hook_file"
    print_success "Uninstalled $name hook"
  else
    print_warning "$name hook exists but is not managed by yaze installer"
    print_info "Leaving it untouched: $hook_file"
  fi
}

status_hook() {
  local hook_file="$1"
  local marker="$2"
  local name="$3"

  if [[ -f "$hook_file" ]]; then
    if grep -q "$marker" "$hook_file" 2>/dev/null; then
      print_success "$name: installed (yaze-managed)"
    else
      print_warning "$name: installed (custom)"
    fi
  else
    print_warning "$name: not installed"
  fi
}

show_status() {
  echo -e "${BLUE}=== Git Hook Status ===${NC}"
  status_hook "$PRECOMMIT_HOOK_FILE" "scripts/pre-commit.sh" "pre-commit"
  status_hook "$PREPUSH_HOOK_FILE" "scripts/pre-push.sh" "pre-push"
  echo ""
  print_info "scripts/pre-commit.sh: $PRECOMMIT_SCRIPT"
  print_info "scripts/pre-push.sh:   $PREPUSH_SCRIPT"
}

main() {
  local command="${1:-install}"

  check_git_repo
  check_hook_dir

  case "$command" in
    install)
      install_precommit_hook
      install_prepush_hook
      ;;
    uninstall)
      uninstall_hook_if_managed "$PRECOMMIT_HOOK_FILE" "scripts/pre-commit.sh" "pre-commit"
      uninstall_hook_if_managed "$PREPUSH_HOOK_FILE" "scripts/pre-push.sh" "pre-push"
      ;;
    status)
      show_status
      ;;
    install-pre-commit)
      install_precommit_hook
      ;;
    install-pre-push)
      install_prepush_hook
      ;;
    uninstall-pre-commit)
      uninstall_hook_if_managed "$PRECOMMIT_HOOK_FILE" "scripts/pre-commit.sh" "pre-commit"
      ;;
    uninstall-pre-push)
      uninstall_hook_if_managed "$PREPUSH_HOOK_FILE" "scripts/pre-push.sh" "pre-push"
      ;;
    --help|-h|help)
      sed -n '1,12p' "$0" | sed 's/^# \{0,1\}//'
      ;;
    *)
      print_error "Unknown command: $command"
      exit 1
      ;;
  esac
}

main "$@"
