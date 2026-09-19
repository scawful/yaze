#!/usr/bin/env bash
# Fails if the configured build routes compiles through a launcher such as
# ccache or sccache. CodeQL's extractor traces real compiler calls; a cache
# hit skips the compiler, so that file would silently drop out of the analysis.
#
# Usage: check-no-compiler-launcher.sh <cmake-build-dir>
set -euo pipefail
build_dir=${1:?usage: $0 <cmake-build-dir>}

# Ninja (single- and multi-config) writes "LAUNCHER = <tool>" into the
# per-target build statements. Multi-config puts them in
# CMakeFiles/impl-<Config>.ninja, not build.ninja.
files=()
while IFS= read -r f; do files+=("$f"); done < <(find "$build_dir" -maxdepth 2 -name '*.ninja' 2>/dev/null)
if [ ${#files[@]} -eq 0 ]; then
  echo "::error::No .ninja files under $build_dir; cannot verify the compiler launcher."
  exit 1
fi

if matches=$(grep -nE '^[[:space:]]*LAUNCHER[[:space:]]*=[[:space:]]*[^[:space:]]' "${files[@]}"); then
  echo "::error::The generated build uses a compiler launcher; CodeQL would miss cached files."
  head -3 <<<"$matches"
  exit 1
fi
echo "No compiler launcher in ${#files[@]} generated .ninja file(s) under $build_dir."
