#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
origin_url="$(git -C "$repo_root" remote get-url origin || true)"

if [[ -z "$origin_url" ]]; then
  echo "[nightly] Unable to resolve origin URL from $repo_root" >&2
  exit 1
fi

nightly_repo="${YAZE_NIGHTLY_REPO:-$HOME/.yaze/nightly/repo}"
nightly_branch="${YAZE_NIGHTLY_BRANCH:-master}"
nightly_build_dir="${YAZE_NIGHTLY_BUILD_DIR:-$nightly_repo/build-nightly}"
nightly_build_type="${YAZE_NIGHTLY_BUILD_TYPE:-RelWithDebInfo}"
nightly_prefix="${YAZE_NIGHTLY_PREFIX:-$HOME/.local/yaze/nightly}"
nightly_component="${YAZE_NIGHTLY_COMPONENT:-yaze}"
nightly_bin_dir="${YAZE_NIGHTLY_BIN_DIR:-$HOME/.local/bin}"
nightly_app_dir="${YAZE_NIGHTLY_APP_DIR:-$HOME/Applications}"
nightly_app_name="${YAZE_NIGHTLY_APP_NAME:-Yaze Nightly.app}"
nightly_app_link="${YAZE_NIGHTLY_APP_LINK:-$nightly_app_dir/$nightly_app_name}"
nightly_ai_runtime="${YAZE_NIGHTLY_AI_RUNTIME:-ON}"
nightly_ai_features="${YAZE_NIGHTLY_AI_FEATURES:-$nightly_ai_runtime}"
nightly_prefer_system_grpc="${YAZE_NIGHTLY_PREFER_SYSTEM_GRPC:-ON}"
nightly_skip_install_rpath="${YAZE_NIGHTLY_SKIP_INSTALL_RPATH:-AUTO}"

if [[ "$nightly_prefer_system_grpc" == "AUTO" ]]; then
  if command -v grpc_cpp_plugin >/dev/null 2>&1 && command -v protoc >/dev/null 2>&1; then
    nightly_prefer_system_grpc="ON"
  else
    nightly_prefer_system_grpc="OFF"
  fi
elif [[ "$nightly_prefer_system_grpc" == "ON" ]]; then
  if ! command -v grpc_cpp_plugin >/dev/null 2>&1 || ! command -v protoc >/dev/null 2>&1; then
    echo "[nightly] System gRPC requested but protoc/grpc_cpp_plugin missing; falling back to CPM build." >&2
    nightly_prefer_system_grpc="OFF"
  fi
fi

if [[ "$nightly_skip_install_rpath" == "AUTO" ]]; then
  if [[ "$(uname -s)" == "Darwin" ]]; then
    nightly_skip_install_rpath="ON"
  else
    nightly_skip_install_rpath="OFF"
  fi
fi

ensure_bin_dir() {
  if [[ -L "$nightly_bin_dir" ]]; then
    local target
    target="$(readlink "$nightly_bin_dir")"
    if [[ -n "$target" ]]; then
      if [[ "$target" != /* ]]; then
        target="$(dirname "$nightly_bin_dir")/$target"
      fi
      if [[ ! -d "$target" ]]; then
        mkdir -p "$target"
      fi
    fi
  fi
  mkdir -p "$nightly_bin_dir"
}

ensure_app_link() {
  if [[ "$(uname -s)" != "Darwin" ]]; then
    return
  fi
  local app_source=""
  for candidate in \
    "$nightly_prefix/current/yaze.app" \
    "$nightly_prefix/current/Yaze.app" \
    "$nightly_prefix/current/Yaze Nightly.app"; do
    if [[ -d "$candidate" ]]; then
      app_source="$candidate"
      break
    fi
  done
  if [[ -z "$app_source" ]]; then
    return
  fi
  mkdir -p "$nightly_app_dir"
  ln -sfn "$app_source" "$nightly_app_link"
}

normalize_app_bundle() {
  local app_dir=""
  if [[ -d "$release_dir/yaze.app" ]]; then
    app_dir="$release_dir/yaze.app"
  elif [[ -d "$release_dir/Yaze.app" ]]; then
    app_dir="$release_dir/Yaze.app"
    ln -sfn "Yaze.app" "$release_dir/yaze.app"
  elif [[ -d "$release_dir/Contents" ]]; then
    mkdir -p "$release_dir/yaze.app"
    mv "$release_dir/Contents" "$release_dir/yaze.app/"
    app_dir="$release_dir/yaze.app"
  fi

  if [[ -n "$app_dir" ]]; then
    local app_bin="$app_dir/Contents/MacOS/yaze"
    if [[ -x "$app_bin" ]]; then
      ln -sfn "$app_bin" "$release_dir/yaze"
    fi
  fi
}

fallback_install() {
  echo "[nightly] Running fallback install copy."
  rm -rf "$release_dir/yaze.app" "$release_dir/assets"

  if [[ -d "$nightly_build_dir/bin/yaze.app" ]]; then
    cp -R "$nightly_build_dir/bin/yaze.app" "$release_dir/"
  elif [[ -f "$nightly_build_dir/bin/yaze" ]]; then
    cp -f "$nightly_build_dir/bin/yaze" "$release_dir/"
  else
    echo "[nightly] Missing yaze build output under $nightly_build_dir/bin" >&2
    return 1
  fi

  if [[ -f "$nightly_build_dir/bin/z3ed" ]]; then
    cp -f "$nightly_build_dir/bin/z3ed" "$release_dir/"
  elif [[ -f "$nightly_build_dir/bin/$nightly_build_type/z3ed" ]]; then
    cp -f "$nightly_build_dir/bin/$nightly_build_type/z3ed" "$release_dir/"
  else
    echo "[nightly] Missing z3ed build output under $nightly_build_dir/bin" >&2
    return 1
  fi

  if [[ -d "$nightly_repo/assets" ]]; then
    cp -R "$nightly_repo/assets" "$release_dir/assets"
  fi

  if [[ -f "$nightly_repo/README.md" ]]; then
    cp -f "$nightly_repo/README.md" "$release_dir/"
  fi
  if [[ -f "$nightly_repo/LICENSE" ]]; then
    cp -f "$nightly_repo/LICENSE" "$release_dir/"
  fi
}

if [[ ! -d "$nightly_repo/.git" ]]; then
  mkdir -p "$(dirname "$nightly_repo")"
  echo "[nightly] Cloning $origin_url -> $nightly_repo"
  git clone --filter=blob:none --recurse-submodules --shallow-submodules \
    --branch "$nightly_branch" "$origin_url" "$nightly_repo"
fi

cd "$nightly_repo"

if [[ -n "$(git status --porcelain)" ]]; then
  echo "[nightly] Repo has local changes: $nightly_repo" >&2
  echo "[nightly] Please clean or stash before running the installer." >&2
  exit 1
fi

echo "[nightly] Updating $nightly_branch"
git fetch origin "$nightly_branch"
git checkout "$nightly_branch"
git pull --ff-only origin "$nightly_branch"

git submodule sync --recursive
# Keep submodules shallow to reduce footprint.
git submodule update --init --recursive --depth 1

cmake_generator="Ninja"
if ! command -v ninja >/dev/null 2>&1; then
  cmake_generator="Unix Makefiles"
fi

echo "[nightly] Configuring build ($cmake_generator, $nightly_build_type)"
cmake -S "$nightly_repo" -B "$nightly_build_dir" -G "$cmake_generator" \
  -DCMAKE_BUILD_TYPE="$nightly_build_type" \
  -DYAZE_ENABLE_GRPC=ON \
  -DYAZE_ENABLE_REMOTE_AUTOMATION=ON \
  -DYAZE_PREFER_SYSTEM_GRPC="$nightly_prefer_system_grpc" \
  -DYAZE_ENABLE_AI_RUNTIME="$nightly_ai_runtime" \
  -DYAZE_ENABLE_AI="$nightly_ai_features" \
  -DCMAKE_SKIP_INSTALL_RPATH="$nightly_skip_install_rpath" \
  -DYAZE_BUILD_TESTS=OFF

echo "[nightly] Building yaze + z3ed"
cmake --build "$nightly_build_dir" --config "$nightly_build_type" --target yaze z3ed

stamp="$(date +%Y%m%d-%H%M%S)"
release_dir="$nightly_prefix/releases/$stamp"

mkdir -p "$nightly_prefix/releases"

echo "[nightly] Installing to $release_dir"
if ! cmake --install "$nightly_build_dir" --prefix "$release_dir" --component "$nightly_component" --config "$nightly_build_type"; then
  echo "[nightly] cmake --install failed; attempting fallback copy." >&2
  fallback_install
fi
normalize_app_bundle
ln -sfn "$release_dir" "$nightly_prefix/current"
ensure_app_link

commit="$(git rev-parse HEAD)"
short_commit="$(git rev-parse --short HEAD)"
version=""
if [[ -f "$nightly_repo/VERSION" ]]; then
  version="$(tr -d ' \n' < "$nightly_repo/VERSION")"
fi
if [[ -n "$version" ]]; then
  describe="v${version}-g${short_commit}"
else
  describe="$(git describe --tags --always 2>/dev/null || echo "$commit")"
fi
{
  echo "commit=$commit"
  echo "describe=$describe"
  if [[ -n "$version" ]]; then
    echo "version=$version"
  fi
  echo "branch=$nightly_branch"
  echo "build_type=$nightly_build_type"
  echo "installed_at=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
} > "$release_dir/BUILD_INFO.txt"

ensure_bin_dir

cat > "$nightly_bin_dir/yaze-nightly" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
prefix="${YAZE_NIGHTLY_PREFIX:-$HOME/.local/yaze/nightly}"
binary="$prefix/current/yaze"
build_info="$prefix/current/BUILD_INFO.txt"

resolve_app() {
  local candidate
  for candidate in \
    "$prefix/current/yaze.app/Contents/MacOS/yaze" \
    "$prefix/current/Yaze.app/Contents/MacOS/yaze" \
    "$prefix/current/Contents/MacOS/yaze"; do
    if [[ -x "$candidate" ]]; then
      echo "$candidate"
      return 0
    fi
  done
  return 1
}

show_help() {
  cat <<'USAGE'
Usage: yaze-nightly [--help] [--version] [args...]

Launches the YAZE GUI build from the nightly install.
Use z3ed-nightly for the CLI/TUI.
USAGE
}

show_version() {
  if [[ -f "$build_info" ]]; then
    describe="$(awk -F= '$1=="describe"{print $2}' "$build_info")"
    commit="$(awk -F= '$1=="commit"{print $2}' "$build_info")"
    if [[ -n "$describe" ]]; then
      echo "yaze-nightly $describe"
      return 0
    fi
    if [[ -n "$commit" ]]; then
      echo "yaze-nightly $commit"
      return 0
    fi
  fi
  echo "yaze-nightly"
}

for arg in "$@"; do
  case "$arg" in
    --help|-h)
      show_help
      exit 0
      ;;
    --version)
      show_version
      exit 0
      ;;
  esac
done

if app="$(resolve_app)"; then
  exec "$app" "$@"
fi
if [[ -x "$binary" ]]; then
  exec "$binary" "$@"
fi
echo "[yaze-nightly] Missing install under $prefix/current. Run scripts/install-nightly.sh." >&2
exit 1
EOF
chmod +x "$nightly_bin_dir/yaze-nightly"

cat > "$nightly_bin_dir/yaze-nightly-grpc" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
prefix="${YAZE_NIGHTLY_PREFIX:-$HOME/.local/yaze/nightly}"
port="${YAZE_GRPC_PORT:-50051}"
binary="$prefix/current/yaze"
build_info="$prefix/current/BUILD_INFO.txt"

resolve_app() {
  local candidate
  for candidate in \
    "$prefix/current/yaze.app/Contents/MacOS/yaze" \
    "$prefix/current/Yaze.app/Contents/MacOS/yaze" \
    "$prefix/current/Contents/MacOS/yaze"; do
    if [[ -x "$candidate" ]]; then
      echo "$candidate"
      return 0
    fi
  done
  return 1
}

show_help() {
  cat <<'USAGE'
Usage: yaze-nightly-grpc [--help] [--version] [args...]

Launches the YAZE GUI build with the gRPC test harness enabled.
Use z3ed-nightly for the CLI/TUI.
USAGE
}

show_version() {
  if [[ -f "$build_info" ]]; then
    describe="$(awk -F= '$1=="describe"{print $2}' "$build_info")"
    commit="$(awk -F= '$1=="commit"{print $2}' "$build_info")"
    if [[ -n "$describe" ]]; then
      echo "yaze-nightly $describe"
      return 0
    fi
    if [[ -n "$commit" ]]; then
      echo "yaze-nightly $commit"
      return 0
    fi
  fi
  echo "yaze-nightly"
}

for arg in "$@"; do
  case "$arg" in
    --help|-h)
      show_help
      exit 0
      ;;
    --version)
      show_version
      exit 0
      ;;
  esac
done

if app="$(resolve_app)"; then
  exec "$app" --enable_test_harness --test_harness_port="$port" "$@"
fi
if [[ -x "$binary" ]]; then
  exec "$binary" --enable_test_harness --test_harness_port="$port" "$@"
fi
echo "[yaze-nightly-grpc] Missing install under $prefix/current. Run scripts/install-nightly.sh." >&2
exit 1
EOF
chmod +x "$nightly_bin_dir/yaze-nightly-grpc"

cat > "$nightly_bin_dir/z3ed-nightly" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
prefix="${YAZE_NIGHTLY_PREFIX:-$HOME/.local/yaze/nightly}"
binary="$prefix/current/z3ed"
if [[ -x "$binary" ]]; then
  exec "$binary" "$@"
fi
echo "[z3ed-nightly] Missing install under $prefix/current. Run scripts/install-nightly.sh." >&2
exit 1
EOF
chmod +x "$nightly_bin_dir/z3ed-nightly"

cat > "$nightly_bin_dir/yaze-mcp-nightly" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
mcp_repo="${YAZE_MCP_REPO:-$HOME/.yaze/yaze-mcp}"
python_bin="$mcp_repo/venv/bin/python"
if [[ ! -x "$python_bin" ]]; then
  echo "[yaze-mcp] Missing venv at $mcp_repo/venv. Run: $mcp_repo/setup.sh" >&2
  exit 1
fi
export YAZE_GRPC_HOST="${YAZE_GRPC_HOST:-localhost}"
export YAZE_GRPC_PORT="${YAZE_GRPC_PORT:-50051}"
export PYTHONPATH="$mcp_repo"
exec "$python_bin" "$mcp_repo/server.py" "$@"
EOF
chmod +x "$nightly_bin_dir/yaze-mcp-nightly"

echo "[nightly] Installed $describe to $release_dir"
if [[ -n "$nightly_app_link" && -L "$nightly_app_link" ]]; then
  echo "[nightly] macOS app link: $nightly_app_link"
fi
if ! command -v yaze-nightly >/dev/null 2>&1; then
  echo "[nightly] Note: $nightly_bin_dir is not on your PATH. Add it to use wrappers."
fi
echo "[nightly] Wrappers: $nightly_bin_dir/yaze-nightly, yaze-nightly-grpc, z3ed-nightly, yaze-mcp-nightly"
