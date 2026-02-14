#!/usr/bin/env bash
# Standard local development workflow for yaze/z3ed.
#
# Commands:
#   all            Build + test + sync + status (default)
#   build          Configure/build yaze + z3ed
#   test           Run ctest preset
#   sync           Sync yaze.app to /Applications and z3ed to PATH link
#   status         Print binary/runtime sync status
#   hooks          Install git hooks (pre-commit + pre-push)
#   release-check  Validate version/changelog protocol
#
# Options:
#   --preset <name>         CMake build preset (default: dev)
#   --ctest-preset <name>   CTest preset (default auto by platform)
#   --jobs <n>              Build parallelism (default: auto)
#   --app-dest <path>       App destination on macOS (default: /Applications/yaze.app)
#   --z3ed-link <path>      PATH symlink target (default: /usr/local/bin/z3ed)
#   --skip-tests            Skip tests for all/build paths
#   --skip-sync             Skip sync for all/build paths
#   --dry-run               Print actions without mutating files

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_header() { echo -e "\n${BLUE}=== $1 ===${NC}"; }
print_info() { echo -e "${BLUE}[info] $1${NC}"; }
print_ok() { echo -e "${GREEN}[ok] $1${NC}"; }
print_warn() { echo -e "${YELLOW}[warn] $1${NC}"; }
print_err() { echo -e "${RED}[err] $1${NC}"; }

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

COMMAND="${1:-all}"
if [[ $# -gt 0 ]]; then
  shift
fi

PRESET="${YAZE_DEV_PRESET:-dev}"
CTEST_PRESET="${YAZE_DEV_CTEST_PRESET:-}"
JOBS="${YAZE_BUILD_JOBS:-0}"
APP_DEST="${YAZE_APP_DEST:-/Applications/yaze.app}"
Z3ED_LINK="${YAZE_Z3ED_LINK:-/usr/local/bin/z3ed}"
SKIP_TESTS=false
SKIP_SYNC=false
DRY_RUN=false

while [[ $# -gt 0 ]]; do
  case "$1" in
    --preset)
      PRESET="$2"; shift 2 ;;
    --ctest-preset)
      CTEST_PRESET="$2"; shift 2 ;;
    --jobs)
      JOBS="$2"; shift 2 ;;
    --app-dest)
      APP_DEST="$2"; shift 2 ;;
    --z3ed-link)
      Z3ED_LINK="$2"; shift 2 ;;
    --skip-tests)
      SKIP_TESTS=true; shift ;;
    --skip-sync)
      SKIP_SYNC=true; shift ;;
    --dry-run)
      DRY_RUN=true; shift ;;
    --help|-h)
      sed -n '1,40p' "$0" | sed 's/^# \{0,1\}//'
      exit 0 ;;
    *)
      print_err "Unknown option: $1"
      exit 2 ;;
  esac
done

detect_default_ctest_preset() {
  case "$(uname -s)" in
    Darwin) echo "mac-ai-quick-editor" ;;
    Linux) echo "fast-lin" ;;
    *) echo "fast" ;;
  esac
}

if [[ -z "$CTEST_PRESET" ]]; then
  CTEST_PRESET="$(detect_default_ctest_preset)"
fi

auto_jobs() {
  if [[ "$JOBS" -gt 0 ]]; then
    echo "$JOBS"
    return
  fi
  if command -v sysctl >/dev/null 2>&1; then
    sysctl -n hw.ncpu 2>/dev/null || echo 8
  elif command -v nproc >/dev/null 2>&1; then
    nproc
  else
    echo 8
  fi
}

run_cmd() {
  if [[ "$DRY_RUN" == true ]]; then
    echo "[dry-run] $*"
  else
    eval "$*"
  fi
}

resolve_z3ed() {
  "$ROOT/scripts/z3ed" --which
}

resolve_yaze_bin() {
  "$ROOT/scripts/yaze" --which
}

resolve_yaze_app_bundle() {
  local yb="$1"
  if [[ "$yb" == *.app/Contents/MacOS/yaze ]]; then
    echo "${yb%/Contents/MacOS/yaze}"
    return 0
  fi

  local c
  for c in \
    "$ROOT/build/bin/Debug/yaze.app" \
    "$ROOT/build/bin/RelWithDebInfo/yaze.app" \
    "$ROOT/build/bin/Release/yaze.app" \
    "$ROOT/build_ai/bin/Debug/yaze.app" \
    "$ROOT/build_ai/bin/RelWithDebInfo/yaze.app" \
    "$ROOT/build_ai/bin/Release/yaze.app"; do
    if [[ -d "$c" ]]; then
      echo "$c"
      return 0
    fi
  done

  return 1
}

status() {
  print_header "Local Workflow Status"

  local repo_version
  repo_version="$(tr -d '[:space:]' < "$ROOT/VERSION")"
  print_info "Repo version: ${repo_version}"

  local z3ed_bin yaze_bin
  z3ed_bin="$(resolve_z3ed)"
  yaze_bin="$(resolve_yaze_bin)"
  print_info "scripts/z3ed -> ${z3ed_bin}"
  print_info "scripts/yaze -> ${yaze_bin}"

  print_info "which -a z3ed:"
  which -a z3ed 2>/dev/null | sed 's/^/  - /' || true

  if [[ -e "$Z3ED_LINK" ]]; then
    print_info "PATH link target (${Z3ED_LINK}): $(readlink "$Z3ED_LINK" 2>/dev/null || echo "$Z3ED_LINK")"
  else
    print_warn "PATH link missing: ${Z3ED_LINK}"
  fi

  if [[ "$(uname -s)" == "Darwin" ]]; then
    if [[ -d "$APP_DEST" ]]; then
      local app_ver
      app_ver="$(defaults read "$APP_DEST/Contents/Info" CFBundleShortVersionString 2>/dev/null || echo "unknown")"
      print_info "Application bundle: ${APP_DEST} (version ${app_ver})"
    else
      print_warn "Application bundle missing: ${APP_DEST}"
    fi
  fi

  print_ok "status complete"
}

build_targets() {
  local njobs
  njobs="$(auto_jobs)"

  print_header "Build"
  print_info "Preset: ${PRESET}"
  print_info "Parallel jobs: ${njobs}"

  run_cmd "cmake --preset '${PRESET}'"
  run_cmd "cmake --build --preset '${PRESET}' --target yaze z3ed --parallel '${njobs}'"
  print_ok "build complete"
}

run_tests() {
  print_header "Tests"
  print_info "CTest preset: ${CTEST_PRESET}"

  if ! ctest --list-presets | rg -F "\"${CTEST_PRESET}\"" >/dev/null 2>&1; then
    print_warn "CTest preset '${CTEST_PRESET}' not found; falling back to 'fast'"
    CTEST_PRESET="fast"
  fi

  run_cmd "ctest --preset '${CTEST_PRESET}' --output-on-failure"
  print_ok "tests complete"
}

sync_runtime() {
  print_header "Sync Runtime"

  local z3ed_bin yaze_bin yaze_app
  z3ed_bin="$(resolve_z3ed)"
  yaze_bin="$(resolve_yaze_bin)"

  if ! yaze_app="$(resolve_yaze_app_bundle "$yaze_bin")"; then
    print_err "Could not resolve yaze.app bundle from binary '${yaze_bin}'"
    return 1
  fi

  print_info "Using z3ed binary: ${z3ed_bin}"
  print_info "Using yaze bundle: ${yaze_app}"

  if [[ "$(uname -s)" == "Darwin" ]]; then
    run_cmd "mkdir -p '$(dirname "$APP_DEST")'"
    run_cmd "rsync -a --delete '${yaze_app}/' '${APP_DEST}/'"
    if [[ "$DRY_RUN" == true ]]; then
      print_info "Would sync yaze.app to ${APP_DEST}"
    else
      print_ok "Synced yaze.app to ${APP_DEST}"
    fi
  else
    print_warn "Non-macOS host: skipping app sync"
  fi

  if [[ -d "$ROOT/build/bin" ]]; then
    run_cmd "cp '${z3ed_bin}' '$ROOT/build/bin/z3ed'"
    if [[ "$DRY_RUN" == true ]]; then
      print_info "Would update $ROOT/build/bin/z3ed"
    else
      print_ok "Updated $ROOT/build/bin/z3ed"
    fi
  fi

  run_cmd "mkdir -p '$(dirname "$Z3ED_LINK")'"
  run_cmd "ln -sfn '${z3ed_bin}' '${Z3ED_LINK}'"
  if [[ "$DRY_RUN" == true ]]; then
    print_info "Would update PATH link ${Z3ED_LINK} -> ${z3ed_bin}"
  else
    print_ok "Updated PATH link ${Z3ED_LINK} -> ${z3ed_bin}"
  fi

  if [[ "$DRY_RUN" == false ]]; then
    if [[ "$(uname -s)" == "Darwin" ]]; then
      local src_hash dst_hash
      src_hash="$(shasum -a 256 "$yaze_app/Contents/MacOS/yaze" | awk '{print $1}')"
      dst_hash="$(shasum -a 256 "$APP_DEST/Contents/MacOS/yaze" | awk '{print $1}')"
      if [[ "$src_hash" == "$dst_hash" ]]; then
        print_ok "yaze.app binary hash verified"
      else
        print_err "yaze.app binary hash mismatch"
        return 1
      fi
    fi

    local z3ed_src z3ed_link
    z3ed_src="$(shasum -a 256 "$z3ed_bin" | awk '{print $1}')"
    z3ed_link="$(shasum -a 256 "$Z3ED_LINK" | awk '{print $1}')"
    if [[ "$z3ed_src" == "$z3ed_link" ]]; then
      print_ok "z3ed link hash verified"
    else
      print_err "z3ed link hash mismatch"
      return 1
    fi
  fi

  print_ok "runtime sync complete"
}

install_hooks() {
  print_header "Hooks"
  run_cmd "scripts/install-git-hooks.sh install"
  print_ok "hooks installed"
}

release_check() {
  print_header "Release Protocol Check"
  run_cmd "scripts/dev/release-version-check.sh"
  print_ok "release protocol check complete"
}

case "$COMMAND" in
  all)
    build_targets
    if [[ "$SKIP_TESTS" == false ]]; then
      run_tests
    else
      print_warn "Skipping tests (--skip-tests)"
    fi
    if [[ "$SKIP_SYNC" == false ]]; then
      sync_runtime
    else
      print_warn "Skipping sync (--skip-sync)"
    fi
    status
    ;;
  build)
    build_targets
    ;;
  test)
    run_tests
    ;;
  sync)
    sync_runtime
    ;;
  status)
    status
    ;;
  hooks)
    install_hooks
    ;;
  release-check)
    release_check
    ;;
  *)
    print_err "Unknown command: $COMMAND"
    exit 2
    ;;
esac
