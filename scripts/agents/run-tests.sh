#!/usr/bin/env bash
# Helper script to configure, build, and run tests for a given CMake preset.
# Usage: scripts/agents/run-tests.sh <preset> [ctest-args...]

set -euo pipefail

setup_ccache() {
  if ! command -v ccache >/dev/null 2>&1; then
    return
  fi

  local tmp_root="${TMPDIR:-/tmp}"
  if [[ -z "${CCACHE_DIR:-}" ]]; then
    export CCACHE_DIR="${tmp_root%/}/ccache"
  fi
  if [[ -z "${CCACHE_TEMPDIR:-}" ]]; then
    export CCACHE_TEMPDIR="${tmp_root%/}/ccache/tmp"
  fi
  mkdir -p "$CCACHE_DIR" "$CCACHE_TEMPDIR" || true
}

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <preset> [ctest-args...]" >&2
  exit 1
fi

PRESET="$1"
shift

setup_ccache
echo "Configuring preset: $PRESET"
cmake --preset "$PRESET" || { echo "Configure failed for preset: $PRESET"; exit 1; }

ROOT_DIR=$(git rev-parse --show-toplevel 2>/dev/null || pwd)
read -r GENERATOR BUILD_CONFIG < <(python - <<'PY' "$PRESET" "$ROOT_DIR"
import json, sys, os
preset = sys.argv[1]
root = sys.argv[2]
with open(os.path.join(root, "CMakePresets.json")) as f:
    data = json.load(f)
configure = {p["name"]: p for p in data.get("configurePresets", [])}
build = {p["name"]: p for p in data.get("buildPresets", [])}

def parents(entry):
    inherits = entry.get("inherits", [])
    if isinstance(inherits, str):
        inherits = [inherits]
    return inherits

def resolve_generator(name, seen=None):
    if seen is None:
        seen = set()
    if name in seen:
        return None
    seen.add(name)
    entry = configure.get(name)
    if not entry:
        return None
    gen = entry.get("generator")
    if gen:
        return gen
    for parent in parents(entry):
        gen = resolve_generator(parent, seen)
        if gen:
            return gen
    return None

generator = resolve_generator(preset)
build_preset = build.get(preset, {})
config = build_preset.get("configuration")
if not config:
    entry = configure.get(preset, {})
    cache = entry.get("cacheVariables", {})
    config = cache.get("CMAKE_BUILD_TYPE", "Debug")

print(generator or "")
print(config or "")
PY
)

echo "Building tests for preset: $PRESET"
BUILD_CMD=(cmake --build --preset "$PRESET")
if [[ "$GENERATOR" == *"Visual Studio"* && -n "$BUILD_CONFIG" ]]; then
  BUILD_CMD+=(--config "$BUILD_CONFIG")
fi
"${BUILD_CMD[@]}" || { echo "Build failed for preset: $PRESET"; exit 1; }

if ctest --preset "$PRESET" --show-only >/dev/null 2>&1; then
  echo "Running tests for preset: $PRESET"
  ctest --preset "$PRESET" "$@"
else
  echo "Test preset '$PRESET' not found, falling back to 'all' tests."
  ctest --preset all "$@"
fi

echo "All tests passed for preset: $PRESET"
