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
# for its WRAM/SRAM symbol maps, which usdasm does not publish. z3dk and
# oracle-of-secrets intentionally track their default branch: they are
# actively developed sibling projects, nothing here is generated from them,
# and agents should read their current state.
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

clone_ref() {
  local name="$1" url="$2" commit="$3" dest="$4" temp
  temp="$(mktemp -d "${dest}.bootstrap.XXXXXX")"
  log "refs: cloning $name -> $dest"
  if [[ -z "$commit" ]]; then
    if ! git clone --depth 1 --quiet "$url" "$temp"; then
      rm -rf -- "$temp"
      return 1
    fi
  elif ! (git init --quiet "$temp" &&
    git -C "$temp" remote add origin "$url" &&
    git -C "$temp" fetch --quiet --depth 1 origin "$commit" &&
    git -C "$temp" checkout --quiet --detach "$commit"); then
    rm -rf -- "$temp"
    return 1
  fi
  if [[ -e "$dest" ]]; then
    echo "[bootstrap] refs: destination appeared during clone: $dest" >&2
    rm -rf -- "$temp"
    return 1
  fi
  mv -- "$temp" "$dest"
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
  local entry name url commit dest worktree_state
  for entry in "${REFS[@]}"; do
    read -r name url commit <<<"$entry"
    dest="$REFS_DIR/$name"
    # A final destination may contain work that this script does not own. New
    # clones use a temporary sibling, so an interrupted bootstrap never needs
    # to delete or replace an unreadable final path.
    if [[ -e "$dest" ]]; then
      worktree_state="$(git -C "$dest" rev-parse --is-inside-work-tree 2>/dev/null || true)"
      if [[ "$worktree_state" != "true" ]]; then
        echo "[bootstrap] refs: refusing to replace unreadable path $dest; move or remove it explicitly" >&2
        return 1
      fi
    fi
    if [[ -z "$commit" ]]; then
      if [[ -e "$dest" ]]; then
        # Unpinned refs track the default branch, so refresh them on warm
        # (snapshotted) containers. Local edits or a failed fetch keep the
        # existing checkout rather than failing setup.
        if [[ -n "$(git -C "$dest" status --porcelain=v1 --untracked-files=all)" ]]; then
          log "refs: $name has local changes; not refreshing"
        elif git -C "$dest" fetch --quiet origin HEAD; then
          if ! git -C "$dest" merge-base --is-ancestor HEAD FETCH_HEAD; then
            log "refs: $name has local or divergent commits; not refreshing"
          elif git -C "$dest" checkout --quiet --detach FETCH_HEAD; then
            log "refs: $name refreshed to $(git -C "$dest" rev-parse --short HEAD)"
          else
            log "refs: $name checkout failed; keeping $(git -C "$dest" rev-parse --short HEAD)"
          fi
        else
          log "refs: $name refresh failed; keeping $(git -C "$dest" rev-parse --short HEAD)"
        fi
      else
        clone_ref "$name" "$url" "" "$dest"
      fi
      continue
    fi
    if [[ -e "$dest" ]] &&
      [[ -n "$(git -C "$dest" status --porcelain=v1 --untracked-files=all)" ]]; then
      echo "[bootstrap] refs: pinned $name has local changes at $dest; refusing to use modified reference input" >&2
      return 1
    fi
    if [[ "$(git -C "$dest" rev-parse HEAD 2>/dev/null)" == "$commit" ]]; then
      log "refs: $name already at pinned ${commit:0:7}"
      continue
    fi
    if [[ ! -e "$dest" ]]; then
      clone_ref "$name" "$url" "$commit" "$dest"
      continue
    fi
    log "refs: fetching $name at pinned ${commit:0:7} -> $dest"
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
      -h|--help)
        # Print the leading comment block (after the shebang) as usage.
        awk 'NR == 1 { next } /^#/ { sub(/^# ?/, ""); print; next } { exit }' \
          "${BASH_SOURCE[0]}"
        return 0
        ;;
      *) echo "[bootstrap] unknown step: $step" >&2; return 2 ;;
    esac
  done
  log "done: ${steps[*]}"
}

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
  main "$@"
fi
