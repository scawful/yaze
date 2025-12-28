#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
origin_url="$(git -C "$repo_root" remote get-url origin || true)"

if [[ -z "$origin_url" ]]; then
  echo "[nightly] Unable to resolve origin URL from $repo_root" >&2
  exit 1
fi

nightly_repo="${YAZE_NIGHTLY_REPO:-$HOME/Code/yaze-nightly}"
nightly_branch="${YAZE_NIGHTLY_BRANCH:-master}"
nightly_build_dir="${YAZE_NIGHTLY_BUILD_DIR:-$nightly_repo/build-nightly}"
nightly_build_type="${YAZE_NIGHTLY_BUILD_TYPE:-RelWithDebInfo}"
nightly_prefix="${YAZE_NIGHTLY_PREFIX:-$HOME/.local/yaze/nightly}"
nightly_component="${YAZE_NIGHTLY_COMPONENT:-yaze}"
nightly_bin_dir="${YAZE_NIGHTLY_BIN_DIR:-$HOME/.local/bin}"

if [[ ! -d "$nightly_repo/.git" ]]; then
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
  -DYAZE_BUILD_TESTS=OFF \
  -DYAZE_ENABLE_AI=OFF \
  -DYAZE_ENABLE_AI_RUNTIME=OFF

echo "[nightly] Building yaze + z3ed"
cmake --build "$nightly_build_dir" --config "$nightly_build_type" --target yaze z3ed

stamp="$(date +%Y%m%d-%H%M%S)"
release_dir="$nightly_prefix/releases/$stamp"

mkdir -p "$nightly_prefix/releases"

echo "[nightly] Installing to $release_dir"
cmake --install "$nightly_build_dir" --prefix "$release_dir" --component "$nightly_component" --config "$nightly_build_type"
ln -sfn "$release_dir" "$nightly_prefix/current"

commit="$(git rev-parse HEAD)"
describe="$(git describe --tags --always 2>/dev/null || echo "$commit")"
{
  echo "commit=$commit"
  echo "describe=$describe"
  echo "branch=$nightly_branch"
  echo "build_type=$nightly_build_type"
  echo "installed_at=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
} > "$release_dir/BUILD_INFO.txt"

mkdir -p "$nightly_bin_dir"

cat > "$nightly_bin_dir/yaze-nightly" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
prefix="${YAZE_NIGHTLY_PREFIX:-$HOME/.local/yaze/nightly}"
exec "$prefix/current/yaze" "$@"
EOF
chmod +x "$nightly_bin_dir/yaze-nightly"

cat > "$nightly_bin_dir/yaze-nightly-grpc" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
prefix="${YAZE_NIGHTLY_PREFIX:-$HOME/.local/yaze/nightly}"
port="${YAZE_GRPC_PORT:-50051}"
exec "$prefix/current/yaze" --enable_test_harness --test_harness_port="$port" "$@"
EOF
chmod +x "$nightly_bin_dir/yaze-nightly-grpc"

cat > "$nightly_bin_dir/z3ed-nightly" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
prefix="${YAZE_NIGHTLY_PREFIX:-$HOME/.local/yaze/nightly}"
exec "$prefix/current/z3ed" "$@"
EOF
chmod +x "$nightly_bin_dir/z3ed-nightly"

cat > "$nightly_bin_dir/yaze-mcp-nightly" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
mcp_repo="${YAZE_MCP_REPO:-$HOME/Code/yaze-mcp}"
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
echo "[nightly] Wrappers: $nightly_bin_dir/yaze-nightly, yaze-nightly-grpc, z3ed-nightly, yaze-mcp-nightly"