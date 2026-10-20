#!/usr/bin/env bash
# ROM guard for yaze: game ROMs must never enter git history.
#
# A retail ROM was committed in 2025 and purged from history on 2026-10-19.
# This check blocks ROM files (.sfc/.smc/.fig/.swc), the purged ROM blob, and
# branches still based on the pre-purge history.
#
# Usage:
#   scripts/check-no-roms.sh [<rev>...]   Scan all history reachable from each
#                                         rev (default: HEAD). Used by CI.
#   scripts/check-no-roms.sh --pre-push   Read git pre-push ref lines on stdin
#                                         and scan only commits not yet on a
#                                         remote. Used by scripts/pre-push.sh.
#
# Exit codes: 0 clean, 1 ROM content found, 2 usage error.

set -uo pipefail

# Blob of the purged zelda3.sfc (retail ROM, SHA-1 6d4f10a8b10e10dbe624cb23cf03b88bb8252973).
PURGED_BLOB=c8f4185ddd465da4d81682c7870756d370d63bf5
# Pre-purge commits that contain that blob. Fresh clones do not have them.
PURGED_COMMITS=(
  476dd1cd1cc2653f52abb27e93b01db18974fdf6
  e36d81f357d4b5d323cb7f07b3fb38684804cc4f
)
ROM_PATTERN='\.([sS][fF][cC]|[sS][mM][cC]|[fF][iI][gG]|[sS][wW][cC])$'
ZERO=0000000000000000000000000000000000000000

status=0

fail() {
  echo "ROM guard: $*" >&2
  status=1
}

# Git's -z output preserves paths containing tabs, newlines, or non-ASCII bytes.
# Escape matched paths only for display; never match Git's quoted display form.
format_rom_paths() {
  local path
  while IFS= read -r -d '' path; do
    if [[ "$path" =~ $ROM_PATTERN ]]; then
      printf '%q\n' "$path"
    fi
  done
}

# Fails if <tip> descends from a pre-purge commit.
check_purged_ancestry() {
  local name="$1" tip="$2" old
  for old in "${PURGED_COMMITS[@]}"; do
    if git merge-base --is-ancestor "$old" "$tip" 2>/dev/null; then
      fail "$name is based on the pre-purge history (contains $old)."
      echo "  Rebase or cherry-pick your commits onto current origin/master." >&2
      return
    fi
  done
}

# Scans commits selected by the rev-list arguments for ROM files and the purged blob.
check_commits() {
  local name="$1"
  shift
  local hits
  # No grep -q: an early exit would SIGPIPE rev-list and pipefail would hide the match.
  if git rev-list --objects "$@" | grep "^$PURGED_BLOB" >/dev/null; then
    fail "$name contains the purged ROM blob $PURGED_BLOB."
  fi
  # A renamed ROM may be deleted before the tip. Treat renames as delete/add and
  # inspect merge diffs so every introduced destination reaches the history scan.
  if ! hits="$(git log -m --format= --name-only --diff-filter=A --no-renames -z "$@" |
    format_rom_paths)"; then
    fail "$name could not scan commit paths."
  fi
  if [[ -n "$hits" ]]; then
    fail "$name adds ROM files:"
    sed 's/^/  /' <<<"$hits" >&2
  fi
}

check_tree() {
  local name="$1" tip="$2" files
  if ! files="$(git ls-tree -r --name-only -z "$tip" | format_rom_paths)"; then
    fail "$name could not scan tree paths."
  fi
  if [[ -n "$files" ]]; then
    fail "$name tracks ROM files:"
    sed 's/^/  /' <<<"$files" >&2
  fi
}

if ! git rev-parse --git-dir >/dev/null 2>&1; then
  echo "ROM guard: not in a git repository" >&2
  exit 2
fi

if [[ "${1:-}" == "--pre-push" ]]; then
  while read -r local_ref local_sha _remote_ref _remote_sha; do
    [[ -z "${local_sha:-}" || "$local_sha" == "$ZERO" ]] && continue
    check_purged_ancestry "$local_ref" "$local_sha"
    check_commits "$local_ref" "$local_sha" --not --remotes
    check_tree "$local_ref" "$local_sha"
  done
else
  if [[ $# -eq 0 ]]; then
    set -- HEAD
  fi
  for rev in "$@"; do
    if ! tip="$(git rev-parse --verify --quiet "$rev^{commit}")"; then
      echo "ROM guard: unknown revision: $rev" >&2
      exit 2
    fi
    check_purged_ancestry "$rev" "$tip"
    check_commits "$rev" "$tip"
    check_tree "$rev" "$tip"
  done
fi

if [[ $status -ne 0 ]]; then
  echo "ROM guard: blocked. yaze must never contain game ROMs; users supply their own." >&2
fi
exit $status
