#!/usr/bin/env bash
# Verify that local nightly installs retain Git provenance from linked worktrees.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
INSTALLER="$ROOT_DIR/scripts/install-nightly-local.sh"

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

assert_build_info_line() {
  local expected="$1"
  local build_info="$2"
  grep -Fx -- "$expected" "$build_info" >/dev/null ||
    fail "missing '$expected' in $build_info"
}

TMP_ROOT="$(mktemp -d)"
trap 'rm -rf "$TMP_ROOT"' EXIT

fixture_repo="$TMP_ROOT/repo"
fixture_worktree="$TMP_ROOT/worktree"
fake_bin="$TMP_ROOT/fake-bin"
nightly_prefix="$TMP_ROOT/nightly"

git init -q "$fixture_repo"
git -C "$fixture_repo" config user.name "Yaze Test"
git -C "$fixture_repo" config user.email "yaze-test@example.invalid"
printf '9.8.7\n' >"$fixture_repo/VERSION"
printf '# Nightly provenance fixture\n' >"$fixture_repo/README.md"
git -C "$fixture_repo" add VERSION README.md
git -C "$fixture_repo" commit -q -m "test fixture"
git -C "$fixture_repo" worktree add -q --detach "$fixture_worktree" HEAD

[[ -f "$fixture_worktree/.git" ]] ||
  fail "fixture is not a linked Git worktree"

expected_commit="$(git -C "$fixture_worktree" rev-parse HEAD)"
expected_short_commit="$(git -C "$fixture_worktree" rev-parse --short HEAD)"

mkdir -p "$fake_bin"
cat >"$fake_bin/cmake" <<'SH'
#!/usr/bin/env bash
set -euo pipefail

if [[ "${1:-}" == "--build" ]]; then
  build_dir="$2"
  mkdir -p "$build_dir/bin"
  : >"$build_dir/bin/yaze"
  : >"$build_dir/bin/z3ed"
  chmod +x "$build_dir/bin/yaze" "$build_dir/bin/z3ed"
  exit 0
fi

if [[ "${1:-}" == "--install" ]]; then
  prefix=""
  while (($#)); do
    if [[ "$1" == "--prefix" ]]; then
      shift
      prefix="${1:-}"
      break
    fi
    shift
  done
  [[ -n "$prefix" ]] || exit 2
  mkdir -p "$prefix"
  : >"$prefix/yaze"
  : >"$prefix/z3ed"
  chmod +x "$prefix/yaze" "$prefix/z3ed"
  exit 0
fi

build_dir=""
while (($#)); do
  if [[ "$1" == "-B" ]]; then
    shift
    build_dir="${1:-}"
    break
  fi
  shift
done
[[ -n "$build_dir" ]] || exit 2
mkdir -p "$build_dir"
SH
chmod +x "$fake_bin/cmake"

run_installer() {
  PATH="$fake_bin:$PATH" \
    YAZE_NIGHTLY_SOURCE_REPO="$fixture_worktree" \
    YAZE_NIGHTLY_BUILD_DIR="$TMP_ROOT/build" \
    YAZE_NIGHTLY_PREFIX="$nightly_prefix" \
    YAZE_NIGHTLY_BIN_DIR="$TMP_ROOT/wrappers" \
    YAZE_NIGHTLY_APP_DIR="$TMP_ROOT/apps" \
    YAZE_NIGHTLY_APP_LINK="$TMP_ROOT/apps/yaze.app" \
    YAZE_NIGHTLY_PREFER_SYSTEM_GRPC=OFF \
    YAZE_NIGHTLY_SKIP_INSTALL_RPATH=OFF \
    "$INSTALLER" >"$TMP_ROOT/install.log"
}

run_installer
build_info="$nightly_prefix/current/BUILD_INFO.txt"
[[ -f "$build_info" ]] || fail "installer did not create BUILD_INFO.txt"
assert_build_info_line "commit=$expected_commit" "$build_info"
assert_build_info_line "describe=v9.8.7-g$expected_short_commit" "$build_info"
assert_build_info_line "dirty=false" "$build_info"
assert_build_info_line "source_repo=$fixture_worktree" "$build_info"

printf 'dirty\n' >>"$fixture_worktree/README.md"
run_installer
build_info="$nightly_prefix/current/BUILD_INFO.txt"
assert_build_info_line "commit=$expected_commit" "$build_info"
assert_build_info_line "describe=v9.8.7-g$expected_short_commit" "$build_info"
assert_build_info_line "dirty=true" "$build_info"

echo "PASS: linked-worktree nightly provenance is recorded"
