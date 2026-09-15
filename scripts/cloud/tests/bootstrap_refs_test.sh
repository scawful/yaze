#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Resolved from this script's absolute directory.
# shellcheck disable=SC1091
source "$SCRIPT_DIR/../bootstrap.sh"

TEST_ROOT="$(mktemp -d)"
trap 'rm -rf -- "$TEST_ROOT"' EXIT

fail() {
  echo "bootstrap refs test failed: $*" >&2
  exit 1
}

SOURCE_REPO="$TEST_ROOT/source"
git init --quiet "$SOURCE_REPO"
git -C "$SOURCE_REPO" config user.name "Yaze Bootstrap Test"
git -C "$SOURCE_REPO" config user.email "bootstrap-test@yaze.invalid"
git -C "$SOURCE_REPO" commit --quiet --allow-empty -m initial

# An existing unreadable final path is user data until proven otherwise.
REFS_DIR="$TEST_ROOT/invalid/refs"
mkdir -p "$REFS_DIR/unpinned"
touch "$REFS_DIR/unpinned/sentinel"
REFS=("unpinned $SOURCE_REPO")
if step_refs; then
  fail "unreadable final path was accepted"
fi
[[ -f "$REFS_DIR/unpinned/sentinel" ]] ||
  fail "unreadable final path was not preserved"

# A clean script-created checkout refreshes to a descendant remote HEAD.
REFS_DIR="$TEST_ROOT/refresh/refs"
REFS=("unpinned $SOURCE_REPO")
step_refs
git -C "$SOURCE_REPO" commit --quiet --allow-empty -m second
SECOND_HEAD="$(git -C "$SOURCE_REPO" rev-parse HEAD)"
step_refs
[[ "$(git -C "$REFS_DIR/unpinned" rev-parse HEAD)" == "$SECOND_HEAD" ]] ||
  fail "clean warm checkout did not refresh"

# Untracked work keeps an unpinned checkout at its existing commit.
touch "$REFS_DIR/unpinned/local-note"
git -C "$SOURCE_REPO" commit --quiet --allow-empty -m third
step_refs
[[ "$(git -C "$REFS_DIR/unpinned" rev-parse HEAD)" == "$SECOND_HEAD" ]] ||
  fail "untracked work did not block refresh"
[[ -f "$REFS_DIR/unpinned/local-note" ]] ||
  fail "untracked work was not preserved"

# Pinned inputs fail closed when their working tree differs from the pin.
REFS_DIR="$TEST_ROOT/pinned/refs"
THIRD_HEAD="$(git -C "$SOURCE_REPO" rev-parse HEAD)"
# Consumed by step_refs from the sourced script.
# shellcheck disable=SC2034
REFS=("pinned $SOURCE_REPO $THIRD_HEAD")
step_refs
touch "$REFS_DIR/pinned/local-note"
if step_refs; then
  fail "modified pinned input was accepted"
fi
[[ -f "$REFS_DIR/pinned/local-note" ]] ||
  fail "modified pinned input was not preserved"

echo "bootstrap refs safety tests passed"
