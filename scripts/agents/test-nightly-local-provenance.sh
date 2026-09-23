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
real_uname="$(command -v uname)"
real_mv="$(command -v mv)"

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

write_binary() {
  cat >"$1" <<'BIN'
#!/usr/bin/env bash
set -euo pipefail
binary="$(basename "$0")"
[[ "${1:-}" == "--version" ]] || exit 8
echo "$binary" >>"$TEST_LOAD_LOG"
[[ "${TEST_LOAD_FAIL:-}" != "$binary" ]] || exit 7
echo "$binary fixture version"
BIN
  chmod +x "$1"
}

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
  install_bin="$prefix"
  if [[ "${TEST_PLATFORM:-Linux}" == "Darwin" ]]; then
    mkdir -p "$prefix/yaze.app/Contents/MacOS" "$prefix/yaze.app/Contents/Resources"
    write_binary "$prefix/yaze.app/Contents/MacOS/yaze"
    : >"$prefix/yaze.app/Contents/Resources/installed-resource"
  else
    install_bin="$prefix/bin"
    mkdir -p "$install_bin"
    write_binary "$install_bin/yaze"
    if [[ "${TEST_EXISTING_ROOT_BINARY:-0}" == "1" ]]; then
      : >"$prefix/yaze"
    fi
  fi
  if [[ "${TEST_INSTALL_MISSING_Z3ED:-0}" != "1" ]]; then
    write_binary "$install_bin/z3ed"
  fi
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

cat >"$fake_bin/uname" <<'SH'
#!/usr/bin/env bash
if [[ "${1:-}" == "-s" ]]; then
  echo "${TEST_PLATFORM:-Linux}"
else
  exec "$REAL_UNAME" "$@"
fi
SH

cat >"$fake_bin/mv" <<'SH'
#!/usr/bin/env bash
# Exercise either installer platform branch using this host's rename flags.
if [[ "${1:-}" == "-fT" || "${1:-}" == "-fh" ]]; then
  [[ "${TEST_ACTIVATION_FAIL:-0}" != "1" ]] || exit 6
  shift
  if [[ "$("$REAL_UNAME" -s)" == "Darwin" ]]; then
    exec "$REAL_MV" -fh "$@"
  fi
  exec "$REAL_MV" -fT "$@"
fi
exec "$REAL_MV" "$@"
SH

cat >"$fake_bin/codesign" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
app="${!#}"
[[ -f "$app/Contents/Resources/installed-resource" ]] || exit 2
if [[ "${1:-}" == "--verify" ]]; then
  echo verify >>"$TEST_SIGN_LOG"
  [[ "${TEST_CODESIGN_FAIL:-}" != "verify" ]] || exit 3
  [[ -f "$app/signed-fixture" ]] || exit 4
else
  echo sign >>"$TEST_SIGN_LOG"
  [[ "${TEST_CODESIGN_FAIL:-}" != "sign" ]] || exit 5
  : >"$app/signed-fixture"
fi
SH
chmod +x "$fake_bin/uname" "$fake_bin/mv" "$fake_bin/codesign"

run_installer() {
  PATH="$fake_bin:$PATH" \
    REAL_UNAME="$real_uname" REAL_MV="$real_mv" \
    TEST_PLATFORM="${TEST_PLATFORM:-Linux}" \
    TEST_INSTALL_MISSING_Z3ED="${TEST_INSTALL_MISSING_Z3ED:-0}" \
    TEST_EXISTING_ROOT_BINARY="${TEST_EXISTING_ROOT_BINARY:-0}" \
    TEST_CODESIGN_FAIL="${TEST_CODESIGN_FAIL:-}" \
    TEST_ACTIVATION_FAIL="${TEST_ACTIVATION_FAIL:-0}" \
    TEST_LOAD_FAIL="${TEST_LOAD_FAIL:-}" \
    TEST_SIGN_LOG="$TMP_ROOT/sign.log" \
    TEST_LOAD_LOG="$TMP_ROOT/load.log" \
    YAZE_NIGHTLY_SOURCE_REPO="$fixture_worktree" \
    YAZE_NIGHTLY_BUILD_DIR="$TMP_ROOT/build" \
    YAZE_NIGHTLY_PREFIX="$nightly_prefix" \
    YAZE_NIGHTLY_BIN_DIR="$TMP_ROOT/wrappers" \
    YAZE_NIGHTLY_APP_DIR="$TMP_ROOT/apps" \
    YAZE_NIGHTLY_APP_LINK="$TMP_ROOT/apps/yaze.app" \
    YAZE_NIGHTLY_PREFER_SYSTEM_GRPC=OFF \
    YAZE_NIGHTLY_SKIP_INSTALL_RPATH=OFF \
    "$INSTALLER" >"$TMP_ROOT/install.log" 2>&1
}

run_installer
build_info="$nightly_prefix/current/BUILD_INFO.txt"
[[ -f "$build_info" ]] || fail "installer did not create BUILD_INFO.txt"
assert_build_info_line "commit=$expected_commit" "$build_info"
assert_build_info_line "describe=v9.8.7-g$expected_short_commit" "$build_info"
assert_build_info_line "dirty=false" "$build_info"
assert_build_info_line "source_repo=$fixture_worktree" "$build_info"
first_release="$(readlink "$nightly_prefix/current")"
for binary in yaze z3ed; do
  [[ "$(readlink "$first_release/$binary")" == "bin/$binary" ]] ||
    fail "Linux $binary wrapper link does not preserve bin layout"
  [[ -f "$first_release/bin/$binary" ]] ||
    fail "Linux $binary was moved out of bin"
done

printf 'dirty\n' >>"$fixture_worktree/README.md"
run_installer
build_info="$nightly_prefix/current/BUILD_INFO.txt"
assert_build_info_line "commit=$expected_commit" "$build_info"
assert_build_info_line "describe=v9.8.7-g$expected_short_commit" "$build_info"
assert_build_info_line "dirty=true" "$build_info"
previous_release="$(readlink "$nightly_prefix/current")"
[[ "$previous_release" != "$first_release" ]] ||
  fail "successive installs reused the same release directory"

assert_previous_release() {
  [[ "$(readlink "$nightly_prefix/current")" == "$previous_release" ]] ||
    fail "failed installation changed current"
  [[ -x "$nightly_prefix/current/yaze" ]] ||
    fail "failed installation damaged the previous executable"
  [[ -f "$nightly_prefix/current/BUILD_INFO.txt" ]] ||
    fail "failed installation damaged previous provenance"
}

if TEST_INSTALL_MISSING_Z3ED=1 run_installer; then
  fail "installer accepted a release without z3ed"
fi
assert_previous_release

if TEST_EXISTING_ROOT_BINARY=1 run_installer; then
  fail "installer replaced an existing root binary while normalizing bin layout"
fi
assert_previous_release

for failure in sign verify; do
  if TEST_PLATFORM=Darwin TEST_CODESIGN_FAIL="$failure" run_installer; then
    fail "installer accepted codesign $failure failure"
  fi
  assert_previous_release
done

for binary in yaze z3ed; do
  if TEST_PLATFORM=Darwin TEST_LOAD_FAIL="$binary" run_installer; then
    fail "installer accepted $binary loader failure"
  fi
  assert_previous_release
done

if TEST_ACTIVATION_FAIL=1 run_installer; then
  fail "installer ignored activation failure"
fi
assert_previous_release

: >"$TMP_ROOT/sign.log"
: >"$TMP_ROOT/load.log"
TEST_PLATFORM=Darwin run_installer
[[ "$(readlink "$nightly_prefix/current")" != "$previous_release" ]] ||
  fail "validated macOS installation was not activated"
[[ "$(tr '\n' ' ' <"$TMP_ROOT/sign.log")" == "sign verify " ]] ||
  fail "macOS installation did not sign then verify"
[[ -f "$nightly_prefix/current/yaze.app/signed-fixture" ]] ||
  fail "activated app was not signed"
[[ -L "$TMP_ROOT/apps/yaze.app" ]] || fail "app link was not installed"
[[ "$(tr '\n' ' ' <"$TMP_ROOT/load.log")" == "yaze z3ed " ]] ||
  fail "validated installation did not load both executables"

echo "PASS: nightly provenance, signing, loader checks, and failure-safe activation"
