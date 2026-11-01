#!/bin/bash
# Copies the non-system dylibs that yaze.app and z3ed load (Homebrew
# openssl@3, libpng, ...) into yaze.app/Contents/Frameworks, points every
# load command at the copies, and re-signs. Without this the packaged
# binaries load /opt/homebrew paths and fail on machines without Homebrew.
#
# Usage: bundle-macos-dylibs.sh <dir containing yaze.app and z3ed>
# Signs with $CODESIGN_IDENTITY (hardened runtime) when set, else ad-hoc.
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 <package-root>"
  exit 2
fi

root="$1"
app="$root/yaze.app"
frameworks="$app/Contents/Frameworks"
app_exe="$app/Contents/MacOS/yaze"
z3ed="$root/z3ed"

if [[ ! -x "$app_exe" ]]; then
  echo "bundle-macos-dylibs: missing $app_exe"
  exit 1
fi

mkdir -p "$frameworks"

is_system_path() {
  case "$1" in
    /usr/lib/* | /System/*) return 0 ;;
  esac
  return 1
}

# Absolute non-system load commands of a Mach-O file, excluding its own id.
external_deps() {
  local file="$1"
  local own_id
  own_id="$(otool -D "$file" | sed -n 2p)"
  otool -L "$file" | tail -n +2 | awk '{print $1}' | while read -r dep; do
    [[ "$dep" == /* ]] || continue
    [[ "$dep" == "$own_id" ]] && continue
    is_system_path "$dep" && continue
    echo "$dep"
  done
}

has_rpath() {
  otool -l "$1" | awk '/cmd LC_RPATH/ {getline; getline; print $2}' |
    grep -qxF "$2"
}

# install_name_tool warns on every edit of a signed file; the files are
# re-signed below, so drop that warning and keep the rest.
edit_load_commands() {
  local output
  if ! output="$(install_name_tool "$@" 2>&1)"; then
    echo "$output"
    return 1
  fi
  [[ -n "$output" ]] || return 0
  grep -v 'will invalidate the code signature' <<< "$output" || true
}

add_rpath() {
  has_rpath "$1" "$2" || edit_load_commands -add_rpath "$2" "$1"
}

# Breadth-first over the binaries and every dylib copied in, so libssl's own
# reference to libcrypto (a Cellar path) is bundled and rewritten too.
queue=("$app_exe")
[[ -x "$z3ed" ]] && queue+=("$z3ed")
index=0
while [[ $index -lt ${#queue[@]} ]]; do
  binary="${queue[$index]}"
  index=$((index + 1))
  deps="$(external_deps "$binary")"
  [[ -n "$deps" ]] || continue
  while read -r dep; do
    name="$(basename "$dep")"
    bundled="$frameworks/$name"
    if [[ ! -e "$bundled" ]]; then
      if [[ ! -f "$dep" ]]; then
        echo "bundle-macos-dylibs: $binary needs missing $dep"
        exit 1
      fi
      echo "Bundling $dep"
      cp -L "$dep" "$bundled"
      chmod u+w "$bundled"
      edit_load_commands -id "@rpath/$name" "$bundled"
      queue+=("$bundled")
    fi
    if [[ "$binary" == "$frameworks"/* ]]; then
      edit_load_commands -change "$dep" "@loader_path/$name" "$binary"
    else
      edit_load_commands -change "$dep" "@rpath/$name" "$binary"
    fi
  done <<< "$deps"
done

add_rpath "$app_exe" "@executable_path/../Frameworks"
if [[ -x "$z3ed" ]]; then
  # z3ed sits beside yaze.app in the DMG; after a drag install it sits
  # wherever the user copied it, so also look in /Applications.
  add_rpath "$z3ed" "@executable_path/yaze.app/Contents/Frameworks"
  add_rpath "$z3ed" "/Applications/yaze.app/Contents/Frameworks"
fi

sign_args=(--force)
if [[ -n "${CODESIGN_IDENTITY:-}" ]]; then
  sign_args+=(--options runtime --timestamp --sign "$CODESIGN_IDENTITY")
else
  sign_args+=(--sign -)
fi

# Sign inside-out: the dylibs, then the bundle that seals them.
find "$frameworks" -type f -name '*.dylib' -print0 |
  while IFS= read -r -d '' dylib; do
    codesign "${sign_args[@]}" "$dylib"
  done
codesign "${sign_args[@]}" "$app"
codesign --verify --deep --strict "$app"
if [[ -x "$z3ed" ]]; then
  codesign "${sign_args[@]}" "$z3ed"
  codesign --verify --strict "$z3ed"
fi

leftover=0
for binary in "$app_exe" "$z3ed" "$frameworks"/*.dylib; do
  [[ -f "$binary" ]] || continue
  remaining="$(external_deps "$binary")"
  if [[ -n "$remaining" ]]; then
    echo "bundle-macos-dylibs: $binary still loads:"
    echo "$remaining"
    leftover=1
  fi
done
exit "$leftover"
