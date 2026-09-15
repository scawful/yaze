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
git init --quiet "$REFS_DIR"
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

# Remote URL forms of one repository compare equal; other repositories do not.
[[ "$(normalize_remote_url https://github.com/Owner/Repo.git)" == \
  "$(normalize_remote_url git@github.com:owner/repo)" ]] ||
  fail "https and scp-style URLs did not normalize equal"
[[ "$(normalize_remote_url ssh://git@github.com/owner/repo/)" == \
  "$(normalize_remote_url https://github.com/owner/repo)" ]] ||
  fail "ssh and https URLs did not normalize equal"
[[ "$(normalize_remote_url https://github.com/owner/fork)" != \
  "$(normalize_remote_url https://github.com/owner/repo)" ]] ||
  fail "different repositories normalized equal"

OTHER_REPO="$TEST_ROOT/other"
git init --quiet "$OTHER_REPO"
git -C "$OTHER_REPO" config user.name "Yaze Bootstrap Test"
git -C "$OTHER_REPO" config user.email "bootstrap-test@yaze.invalid"
git -C "$OTHER_REPO" commit --quiet --allow-empty -m other

# A checkout whose origin is a different repository is refused, pinned or not.
REFS_DIR="$TEST_ROOT/origin/refs"
REFS=("unpinned $SOURCE_REPO")
step_refs
ORIGIN_HEAD="$(git -C "$REFS_DIR/unpinned" rev-parse HEAD)"
REFS=("unpinned $OTHER_REPO")
if step_refs; then
  fail "unpinned checkout with a different origin was accepted"
fi
[[ "$(git -C "$REFS_DIR/unpinned" rev-parse HEAD)" == "$ORIGIN_HEAD" ]] ||
  fail "wrong-origin unpinned checkout was moved"
REFS=("unpinned $OTHER_REPO $(git -C "$OTHER_REPO" rev-parse HEAD)")
if step_refs; then
  fail "pinned checkout with a different origin was accepted"
fi

# Re-pinning moves a checkout bootstrap placed, but never one carrying a
# local commit on its detached HEAD.
REFS_DIR="$TEST_ROOT/repin/refs"
REFS=("pinned $SOURCE_REPO $THIRD_HEAD")
step_refs
git -C "$SOURCE_REPO" commit --quiet --allow-empty -m fourth
FOURTH_HEAD="$(git -C "$SOURCE_REPO" rev-parse HEAD)"
REFS=("pinned $SOURCE_REPO $FOURTH_HEAD")
step_refs
[[ "$(git -C "$REFS_DIR/pinned" rev-parse HEAD)" == "$FOURTH_HEAD" ]] ||
  fail "bootstrap-owned pinned checkout did not re-pin"

git -C "$REFS_DIR/pinned" -c user.name=Local -c user.email=local@yaze.invalid \
  commit --quiet --allow-empty -m "local work"
LOCAL_HEAD="$(git -C "$REFS_DIR/pinned" rev-parse HEAD)"
git -C "$SOURCE_REPO" commit --quiet --allow-empty -m fifth
# Consumed by step_refs from the sourced script.
# shellcheck disable=SC2034
REFS=("pinned $SOURCE_REPO $(git -C "$SOURCE_REPO" rev-parse HEAD)")
if step_refs; then
  fail "pinned checkout with a local commit was moved"
fi
[[ "$(git -C "$REFS_DIR/pinned" rev-parse HEAD)" == "$LOCAL_HEAD" ]] ||
  fail "local commit on a pinned checkout was orphaned"

# A symlink at the destination is user data, not a checkout this script owns.
REFS_DIR="$TEST_ROOT/symlink/refs"
mkdir -p "$REFS_DIR"
REAL_DIR="$TEST_ROOT/symlink/elsewhere"
git clone --quiet "$SOURCE_REPO" "$REAL_DIR"
ln -s "$REAL_DIR" "$REFS_DIR/unpinned"
# shellcheck disable=SC2034
REFS=("unpinned $SOURCE_REPO")
if step_refs; then
  fail "symlinked destination was accepted"
fi
[[ -L "$REFS_DIR/unpinned" ]] || fail "symlinked destination was replaced"

# An unpinned checkout this script did not create is left alone.
REFS_DIR="$TEST_ROOT/foreign/refs"
mkdir -p "$REFS_DIR"
git clone --quiet "$SOURCE_REPO" "$REFS_DIR/unpinned"
FOREIGN_HEAD="$(git -C "$REFS_DIR/unpinned" rev-parse HEAD)"
git -C "$SOURCE_REPO" commit --quiet --allow-empty -m sixth
# shellcheck disable=SC2034
REFS=("unpinned $SOURCE_REPO")
step_refs
[[ "$(git -C "$REFS_DIR/unpinned" rev-parse HEAD)" == "$FOREIGN_HEAD" ]] ||
  fail "a checkout bootstrap did not create was refreshed"

# A failed clone of an unpinned ref leaves the rest of the environment usable.
REFS_DIR="$TEST_ROOT/partial/refs"
# shellcheck disable=SC2034
REFS=("missing $TEST_ROOT/does-not-exist" "unpinned $SOURCE_REPO")
# Run in a subshell with the status captured afterwards: calling step_refs in
# a condition would disable errexit and hide an abort.
set +e
( set -e; step_refs )
partial_status=$?
set -e
[[ "$partial_status" -eq 0 ]] ||
  fail "one failed unpinned clone aborted the bootstrap (status $partial_status)"
[[ ! -e "$REFS_DIR/missing" ]] || fail "failed clone left a destination behind"
[[ -d "$REFS_DIR/unpinned/.git" ]] ||
  fail "refs after a failed unpinned clone were skipped"
[[ -z "$(find "$REFS_DIR" -maxdepth 1 -name '*.bootstrap.*' -print -quit)" ]] ||
  fail "failed clone left a temporary directory behind"

echo "bootstrap refs safety tests passed"
