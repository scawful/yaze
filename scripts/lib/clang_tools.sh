#!/usr/bin/env bash
# Shared clang tooling discovery for yaze quality scripts.
#
# Source this file; it defines functions and changes no global state:
#
#   yaze_clang_pinned_version   Version string from .clang-format-version.
#   yaze_clang_pinned_major     Major component of that pin.
#   yaze_find_clang_format      Print a clang-format command, or return 1.
#   yaze_find_clang_tidy        Print a clang-tidy command, or return 1.
#   yaze_find_cppcheck          Print a cppcheck command, or return 1.
#   yaze_warn_clang_version     Warn (never fail) on a major-version mismatch.
#
# .clang-format-version is the single pin shared by CI, .pre-commit-config.yaml,
# the CMake format targets, and these scripts. Discovery prefers the pinned
# major (clang-format-22) and falls back to an unsuffixed binary, because most
# package managers ship only one. A different major still runs; it only warns,
# since its output can differ from CI for the same .clang-format.
#
# Overrides, mainly for tests and unusual installs:
#   YAZE_CLANG_FORMAT, YAZE_CLANG_TIDY, YAZE_CPPCHECK
#     Set to a path to force that binary. Set to the empty string to declare the
#     tool unavailable.
#   YAZE_CLANG_VERSION_FILE
#     Alternate location for the version pin.

yaze_clang_tools_root() {
  local lib_dir
  lib_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  (cd "${lib_dir}/../.." && pwd)
}

yaze_clang_pinned_version() {
  local version_file
  version_file="${YAZE_CLANG_VERSION_FILE:-$(yaze_clang_tools_root)/.clang-format-version}"
  [[ -r "$version_file" ]] || return 1
  local pin
  pin="$(tr -d '[:space:]' <"$version_file")"
  [[ -n "$pin" ]] || return 1
  printf '%s\n' "$pin"
}

yaze_clang_pinned_major() {
  local pin
  pin="$(yaze_clang_pinned_version)" || return 1
  printf '%s\n' "${pin%%.*}"
}

# Honour an override if the variable is set at all, so `YAZE_CLANG_TIDY=` means
# "treat clang-tidy as missing" instead of "fall back to discovery".
# Returns 0 after printing the override, 1 for a deliberate "missing", 2 when no
# override is set.
_yaze_tool_override() {
  local name="$1"
  [[ -n "${!name+set}" ]] || return 2
  [[ -n "${!name}" ]] || return 1
  printf '%s\n' "${!name}"
}

# Resolve a clang tool by preferred name, then base name, then the Homebrew LLVM
# prefix (keg-only on macOS, so its binaries are usually off PATH).
_yaze_find_clang_tool() {
  local base="$1"
  local preferred="${2:-}"
  local candidate
  for candidate in "$preferred" "$base"; do
    [[ -n "$candidate" ]] || continue
    if command -v "$candidate" >/dev/null 2>&1; then
      command -v "$candidate"
      return 0
    fi
  done

  if [[ "$(uname -s)" == "Darwin" ]] && command -v brew >/dev/null 2>&1; then
    local llvm_prefix
    llvm_prefix="$(brew --prefix llvm 2>/dev/null || true)"
    if [[ -n "$llvm_prefix" && -x "${llvm_prefix}/bin/${base}" ]]; then
      printf '%s\n' "${llvm_prefix}/bin/${base}"
      return 0
    fi
  fi

  return 1
}

yaze_find_clang_format() {
  local status=0
  _yaze_tool_override YAZE_CLANG_FORMAT || status=$?
  [[ "$status" -eq 2 ]] || return "$status"

  local major
  major="$(yaze_clang_pinned_major || true)"
  _yaze_find_clang_tool clang-format "${major:+clang-format-${major}}"
}

yaze_find_clang_tidy() {
  local status=0
  _yaze_tool_override YAZE_CLANG_TIDY || status=$?
  [[ "$status" -eq 2 ]] || return "$status"

  local major
  major="$(yaze_clang_pinned_major || true)"
  _yaze_find_clang_tool clang-tidy "${major:+clang-tidy-${major}}"
}

yaze_find_cppcheck() {
  local status=0
  _yaze_tool_override YAZE_CPPCHECK || status=$?
  [[ "$status" -eq 2 ]] || return "$status"

  command -v cppcheck
}

yaze_clang_tool_major() {
  local output
  output="$("$1" --version 2>/dev/null)" || return 1
  local major
  major="$(printf '%s\n' "$output" | sed -n 's/.*version \([0-9][0-9]*\).*/\1/p' | head -n 1)"
  [[ -n "$major" ]] || return 1
  printf '%s\n' "$major"
}

yaze_warn_clang_version() {
  local tool="$1"
  local label="${2:-$(basename "$1")}"
  local want have
  want="$(yaze_clang_pinned_major)" || return 0
  have="$(yaze_clang_tool_major "$tool")" || return 0
  if [[ "$have" != "$want" ]]; then
    echo "[warn] ${label} is major ${have}; this repo pins major ${want}" \
      "(.clang-format-version). Output may differ from CI." >&2
  fi
  return 0
}
