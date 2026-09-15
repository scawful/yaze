#!/usr/bin/env bash
# Prepare a Linux cloud agent container (Claude Code web, Codex cloud, Cursor
# background agents) to build and test yaze without a ROM.
#
# Usage: scripts/cloud/bootstrap.sh [steps...]
#
# Steps (default: deps submodules refs):
#   deps        apt packages for the no-gRPC `lin-test` preset
#   submodules  shallow-init ext/ submodules
#   refs        shallow-clone reference repos into $YAZE_REFS_DIR (~/refs)
#   configure   cmake --preset $YAZE_CLOUD_PRESET (downloads pinned deps)
#   build       build yaze_test_unit
#   test        ctest -L '^unit$' on the preset build (ROM tests skip)
#   all         every step above, in order
#
# Environment:
#   YAZE_REFS_DIR       reference checkout root (default: ~/refs)
#   YAZE_CLOUD_PRESET   configure/build preset (default: lin-test)
#   YAZE_CLOUD_CONFIG   ctest configuration (default: RelWithDebInfo)
#   YAZE_BUILD_JOBS     build parallelism (default: 4, the repo-wide cap)
#
# Setup-script time limits are tight on some platforms, so the default steps
# only install and clone. Run `configure build test` inside the session.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
REFS_DIR="${YAZE_REFS_DIR:-$HOME/refs}"
PRESET="${YAZE_CLOUD_PRESET:-lin-test}"
CONFIG="${YAZE_CLOUD_CONFIG:-RelWithDebInfo}"
JOBS="${YAZE_BUILD_JOBS:-4}"

# Mirrors .github/workflows/scripts/linux-ci-packages.txt minus the gRPC,
# protobuf, boost, and abseil packages that lin-test does not use. Abseil is
# fetched at configure time so every agent builds the pinned version.
APT_PACKAGES=(
  build-essential cmake ninja-build pkg-config ccache git ca-certificates
  python3 python3-dev
  libgtk-3-dev libglew-dev libpng-dev libwavpack-dev libdbus-1-dev
  libasound2-dev libpulse-dev libaudio-dev
  libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxinerama-dev
  libxi-dev libxss-dev libxxf86vm-dev libxkbcommon-dev libwayland-dev
  libdecor-0-dev
)

# Public reference repos: "name url [commit]". None contain ROM images
# (Oracle of Secrets ignores Roms/*); never add a source that ships
# copyrighted ROM or leaked data. usdasm is pinned: later upstream commits use
# the Futaba format that scripts/agents/alttp_reference.py cannot parse, and
# docs/internal/zelda3 tables are generated from this commit. jpdasm is pinned
# for its WRAM/SRAM symbol maps, which usdasm does not publish.
REFS=(
  "usdasm https://github.com/spannerisms/usdasm.git 835b15b91fc93a635fbe319da045c7d0a034bb12"
  "jpdasm https://github.com/spannerisms/jpdasm.git 4535f694752d1469ede65083e986ce2101945264"
  "z3dk https://github.com/scawful/z3dk.git"
  "oracle-of-secrets https://github.com/scawful/Oracle-of-Secrets.git"
)

log() { printf '\n[bootstrap] %s\n' "$*"; }

as_root() {
  if [[ "$(id -u)" -eq 0 ]]; then
    "$@"
  elif command -v sudo >/dev/null 2>&1; then
    sudo "$@"
  else
    echo "[bootstrap] need root or sudo for: $*" >&2
    return 1
  fi
}

step_deps() {
  if ! command -v apt-get >/dev/null 2>&1; then
    log "apt-get not found; skipping deps (install the packages listed in ${BASH_SOURCE[0]})"
    return 0
  fi
  log "installing ${#APT_PACKAGES[@]} apt packages"
  as_root env DEBIAN_FRONTEND=noninteractive apt-get update -qq
  as_root env DEBIAN_FRONTEND=noninteractive apt-get install -y -qq \
    --no-install-recommends "${APT_PACKAGES[@]}"
}

step_submodules() {
  log "initializing submodules (shallow)"
  if ! git -C "$REPO_ROOT" submodule update --init --recursive --depth 1; then
    log "shallow submodule fetch failed; retrying full depth"
    git -C "$REPO_ROOT" submodule update --init --recursive
  fi
}

step_refs() {
  mkdir -p "$REFS_DIR"
  local entry name url commit dest
  for entry in "${REFS[@]}"; do
    read -r name url commit <<<"$entry"
    dest="$REFS_DIR/$name"
    # An interrupted clone/fetch can leave a directory git cannot read; start
    # that ref over instead of failing later with an obscure error.
    if [[ -e "$dest" ]] && ! git -C "$dest" rev-parse HEAD >/dev/null 2>&1; then
      log "refs: $name at $dest is incomplete; re-fetching"
      rm -rf -- "$dest"
    fi
    if [[ -z "$commit" ]]; then
      if [[ -d "$dest/.git" ]]; then
        log "refs: $name already present at $dest"
      else
        log "refs: cloning $name -> $dest"
        git clone --depth 1 --quiet "$url" "$dest"
      fi
      continue
    fi
    if [[ "$(git -C "$dest" rev-parse HEAD 2>/dev/null)" == "$commit" ]]; then
      log "refs: $name already at pinned ${commit:0:7}"
      continue
    fi
    log "refs: fetching $name at pinned ${commit:0:7} -> $dest"
    if [[ ! -d "$dest/.git" ]]; then
      git init --quiet "$dest"
      git -C "$dest" remote add origin "$url"
    fi
    git -C "$dest" fetch --quiet --depth 1 origin "$commit"
    git -C "$dest" checkout --quiet --detach "$commit"
  done
}

step_configure() {
  log "configuring preset $PRESET"
  cmake --preset "$PRESET"
}

step_build() {
  log "building yaze_test_unit (preset $PRESET, $JOBS jobs)"
  cmake --build --preset "$PRESET" --target yaze_test_unit --parallel "$JOBS"
}

step_test() {
  # Label ^unit$ selects exactly yaze_test_unit's discovered tests. The
  # fast-lin preset (^stable$) also matches CLI suites this step does not
  # build. ROM-dependent cases skip without YAZE_TEST_ROM_* paths.
  log "running yaze_test_unit via ctest (label ^unit\$, $JOBS jobs)"
  ctest --test-dir "$REPO_ROOT/build/presets/$PRESET" -C "$CONFIG" \
    -L '^unit$' --output-on-failure --parallel "$JOBS"
}

main() {
  local steps=("$@")
  if [[ ${#steps[@]} -eq 0 ]]; then
    steps=(deps submodules refs)
  elif [[ "${steps[0]}" == "all" ]]; then
    steps=(deps submodules refs configure build test)
  fi

  cd "$REPO_ROOT"
  local step
  for step in "${steps[@]}"; do
    case "$step" in
      deps) step_deps ;;
      submodules) step_submodules ;;
      refs) step_refs ;;
      configure) step_configure ;;
      build) step_build ;;
      test) step_test ;;
      -h|--help) sed -n '2,24p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; return 0 ;;
      *) echo "[bootstrap] unknown step: $step" >&2; return 2 ;;
    esac
  done
  log "done: ${steps[*]}"
}

main "$@"
