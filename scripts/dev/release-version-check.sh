#!/usr/bin/env bash
# Validate release/versioning hygiene.
#
# Usage:
#   scripts/dev/release-version-check.sh [--staged]
#
# Checks:
#   - VERSION exists and matches x.y.z
#   - If VERSION changed, CHANGELOG.md must also change
#   - If VERSION changed, CHANGELOG.md must include a section heading for that version

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_info() { echo -e "${BLUE}$1${NC}"; }
print_ok() { echo -e "${GREEN}$1${NC}"; }
print_warn() { echo -e "${YELLOW}$1${NC}"; }
print_err() { echo -e "${RED}$1${NC}"; }

MODE="working"
if [[ "${1:-}" == "--staged" ]]; then
  MODE="staged"
fi

REPO_ROOT="$(git rev-parse --show-toplevel 2>/dev/null || true)"
if [[ -z "$REPO_ROOT" ]]; then
  print_err "Not in a git repository"
  exit 1
fi
cd "$REPO_ROOT"

if [[ ! -f VERSION ]]; then
  print_err "Missing VERSION file"
  exit 1
fi

VERSION_VALUE="$(tr -d '[:space:]' < VERSION)"
if [[ ! "$VERSION_VALUE" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  print_err "VERSION must match x.y.z (got: '$VERSION_VALUE')"
  exit 1
fi

if [[ "$MODE" == "staged" ]]; then
  mapfile -t CHANGED < <(git diff --cached --name-only --diff-filter=ACMR)
else
  mapfile -t CHANGED < <(git diff --name-only --diff-filter=ACMR HEAD)
fi

has_changed() {
  local needle="$1"
  local f
  for f in "${CHANGED[@]}"; do
    if [[ "$f" == "$needle" ]]; then
      return 0
    fi
  done
  return 1
}

VERSION_CHANGED=false
CHANGELOG_CHANGED=false
DOC_CHANGELOG_CHANGED=false

has_changed "VERSION" && VERSION_CHANGED=true
has_changed "CHANGELOG.md" && CHANGELOG_CHANGED=true
has_changed "docs/public/reference/changelog.md" && DOC_CHANGELOG_CHANGED=true

if [[ "$VERSION_CHANGED" == true ]]; then
  print_info "VERSION changed -> enforcing changelog rules"

  if [[ "$CHANGELOG_CHANGED" != true ]]; then
    print_err "VERSION changed but CHANGELOG.md was not updated"
    exit 1
  fi

  VERSION_HEADING_REGEX="^##[[:space:]]+${VERSION_VALUE//./\\.}([[:space:]]|\$)"
  if ! rg -n "$VERSION_HEADING_REGEX" CHANGELOG.md >/dev/null 2>&1; then
    print_err "CHANGELOG.md is missing a section heading for VERSION ${VERSION_VALUE}"
    print_info "Expected heading example: ## ${VERSION_VALUE} (Month YYYY)"
    exit 1
  fi

  if [[ "$DOC_CHANGELOG_CHANGED" != true ]]; then
    print_warn "VERSION changed but docs/public/reference/changelog.md was not updated"
    print_warn "This is not a hard error, but public changelog drift is likely."
  fi
fi

print_ok "release-version-check passed (${MODE})"
