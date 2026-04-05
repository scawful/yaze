#!/usr/bin/env bash
set -euo pipefail

# Extract a single release section from docs/public/release-notes.md.
# Usage:
#   scripts/release/extract-release-notes.sh v0.7.0 docs/public/release-notes.md

if [[ $# -ne 2 ]]; then
  echo "Usage: $0 <version-tag> <release-notes-file>" >&2
  exit 2
fi

version_tag="$1"
notes_file="$2"

if [[ ! -f "$notes_file" ]]; then
  echo "Release notes file not found: $notes_file" >&2
  exit 1
fi

version_plain="${version_tag#v}"
target_heading="## v${version_plain}"

awk -v target="${target_heading}" '
BEGIN {
  found = 0
}
{
  if ($0 ~ /^## v[0-9]+\.[0-9]+\.[0-9]+/) {
    if (found == 1 && index($0, target) != 1) {
      exit
    }
    if (index($0, target) == 1) {
      found = 1
    }
  }

  if (found == 1) {
    print $0
  }
}
END {
  if (found == 0) {
    exit 3
  }
}
' "$notes_file" || {
  code=$?
  if [[ $code -eq 3 ]]; then
    echo "Release section not found for ${target_heading} in ${notes_file}" >&2
  fi
  exit $code
}
