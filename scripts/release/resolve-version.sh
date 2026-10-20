#!/usr/bin/env bash
# Keep VERSION numeric; carry a prerelease suffix separately through CMake.
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "Usage: $0 <version-tag> <VERSION-file>" >&2
  exit 2
fi

version_plain="${1#v}"
if [[ ! "$version_plain" =~ ^([0-9]+\.[0-9]+\.[0-9]+)(-[0-9A-Za-z-]+(\.[0-9A-Za-z-]+)*)?$ || "$version_plain" == *- ]]; then
  echo "Invalid release version: $1" >&2
  exit 1
fi
version_base="${BASH_REMATCH[1]}"
version_suffix="${BASH_REMATCH[2]:-}"
repo_version="$(tr -d '[:space:]' < "$2")"
if [[ "$repo_version" != "$version_base" ]]; then
  echo "VERSION file ($repo_version) does not match release base ($version_base)" >&2
  exit 1
fi

prerelease=false
if [[ -n "$version_suffix" ]]; then
  prerelease=true
fi
printf 'version_tag=v%s\nversion_plain=%s\nversion_suffix=%s\nprerelease=%s\n' \
  "$version_plain" "$version_plain" "$version_suffix" "$prerelease"
