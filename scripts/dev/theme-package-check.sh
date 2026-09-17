#!/usr/bin/env bash
set -euo pipefail

repo_root="$(git rev-parse --show-toplevel)"
manifest="$repo_root/assets/themes/distributable-themes.txt"
forbidden_theme="monokai_pro.theme"

if [[ ! -f "$manifest" ]]; then
  echo "Missing distributable theme manifest: $manifest" >&2
  exit 1
fi

tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT
manifest_names="$tmp_dir/manifest"
tracked_names="$tmp_dir/tracked"

while IFS= read -r name || [[ -n "$name" ]]; do
  name="${name%$'\r'}"
  [[ -z "$name" || "$name" == \#* ]] && continue
  if [[ "$name" == */* || "$name" != *.theme ]]; then
    echo "Invalid theme manifest entry: $name" >&2
    exit 1
  fi
  if [[ "$name" == "$forbidden_theme" ]]; then
    echo "Forbidden theme is listed for distribution: $name" >&2
    exit 1
  fi
  if [[ ! -f "$repo_root/assets/themes/$name" ]]; then
    echo "Manifest theme does not exist: $name" >&2
    exit 1
  fi
  printf '%s\n' "$name" >>"$manifest_names"
done <"$manifest"

if [[ ! -s "$manifest_names" ]]; then
  echo "Theme manifest is empty" >&2
  exit 1
fi

if [[ "$(sort "$manifest_names" | uniq -d)" != "" ]]; then
  echo "Theme manifest contains duplicate entries" >&2
  exit 1
fi
sort -o "$manifest_names" "$manifest_names"

git -C "$repo_root" ls-files 'assets/themes/*.theme' |
  awk -F/ '{print $NF}' |
  sort >"$tracked_names"

if git -C "$repo_root" ls-files --error-unmatch \
  "assets/themes/$forbidden_theme" >/dev/null 2>&1; then
  echo "Forbidden theme is tracked by Git: $forbidden_theme" >&2
  exit 1
fi

if ! diff -u "$tracked_names" "$manifest_names"; then
  echo "Tracked themes and the distributable manifest differ" >&2
  exit 1
fi

for artifact_root in "$@"; do
  if [[ ! -e "$artifact_root" ]]; then
    echo "Package inspection path does not exist: $artifact_root" >&2
    exit 1
  fi
  if find "$artifact_root" -name "$forbidden_theme" -print -quit |
    grep -q .; then
    echo "Forbidden theme found in package: $artifact_root" >&2
    exit 1
  fi
  package_names="$tmp_dir/package"
  find "$artifact_root" -type f -name '*.theme' -exec basename {} \; |
    sort >"$package_names"
  if ! diff -u "$manifest_names" "$package_names"; then
    echo "Packaged themes and the distributable manifest differ: $artifact_root" >&2
    exit 1
  fi
done

echo "Theme package manifest is valid ($# package path(s) inspected)"
