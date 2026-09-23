#!/usr/bin/env bash
set -euo pipefail

usage() {
  echo "Usage: $0 [--install-deb] --expected-version <version> <package.tar.gz|package.deb>"
}

install_deb=false
expected_version=""
package_path=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --install-deb)
      install_deb=true
      shift
      ;;
    --expected-version)
      [[ $# -ge 2 ]] || { echo "Missing value for --expected-version"; usage; exit 2; }
      expected_version="$2"
      shift 2
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    --*)
      echo "Unknown argument: $1"
      usage
      exit 2
      ;;
    *)
      if [[ -n "$package_path" ]]; then
        echo "Only one package may be tested at a time"
        usage
        exit 2
      fi
      package_path="$1"
      shift
      ;;
  esac
done

expected_version="${expected_version#v}"
if [[ -z "$expected_version" || -z "$package_path" ]]; then
  usage
  exit 2
fi
if [[ ! -f "$package_path" ]]; then
  echo "Package not found: $package_path"
  exit 2
fi

run_smoke_test() {
  local label="$1"
  local working_directory="$2"
  local binary="$3"
  local argument="$4"
  local expected_output="${5:-}"
  local output
  local status=0

  if command -v timeout >/dev/null 2>&1; then
    output=$(cd "$working_directory" && timeout 30s "$binary" "$argument" 2>&1) || status=$?
  else
    output=$(cd "$working_directory" && "$binary" "$argument" 2>&1) || status=$?
  fi

  echo "$output"
  if [[ $status -ne 0 ]]; then
    echo "$label failed with status $status"
    exit 1
  fi
  if [[ -z "${output//[[:space:]]/}" ]]; then
    echo "$label produced no output"
    exit 1
  fi
  if [[ -n "$expected_output" && "$output" != "$expected_output" ]]; then
    echo "$label output mismatch: expected '$expected_output', found '$output'"
    exit 1
  fi
}

check_dependencies() {
  local binary="$1"
  local output
  local status=0

  command -v ldd >/dev/null 2>&1 || {
    echo "ldd is required to inspect packaged binaries"
    exit 1
  }

  output=$(ldd "$binary" 2>&1) || status=$?
  echo "$output"
  if grep -Eq '(^|[[:space:]])not found($|[[:space:]])|=>[[:space:]]+not found' <<<"$output"; then
    echo "Unresolved shared-library dependency for $binary"
    exit 1
  fi
  if [[ $status -ne 0 ]] && ! grep -Eq 'not a dynamic executable|statically linked' <<<"$output"; then
    echo "ldd failed for $binary with status $status"
    exit 1
  fi
}

if [[ "$install_deb" == true ]]; then
  if [[ "$package_path" != *.deb ]]; then
    echo "--install-deb requires a .deb package"
    exit 2
  fi
  for command_name in apt-get dpkg-deb dpkg-query realpath sudo; do
    command -v "$command_name" >/dev/null 2>&1 || {
      echo "$command_name is required for the DEB installation smoke test"
      exit 1
    }
  done

  if dpkg-query -W yaze >/dev/null 2>&1; then
    echo "A yaze package record already exists; refusing to alter it"
    exit 1
  fi
  for existing_path in /usr/bin/yaze /usr/bin/z3ed /usr/share/yaze /usr/share/doc/yaze; do
    if [[ -e "$existing_path" || -L "$existing_path" ]]; then
      echo "Existing path would be overwritten by the package: $existing_path"
      exit 1
    fi
  done

  package_path="$(realpath "$package_path")"
  deb_package="$(dpkg-deb --field "$package_path" Package)"
  deb_version="$(dpkg-deb --field "$package_path" Version)"
  if [[ "$deb_package" != "yaze" ]]; then
    echo "DEB Package must be 'yaze', found '$deb_package'"
    exit 1
  fi
  if [[ "${deb_version#v}" != "$expected_version" ]]; then
    echo "DEB Version mismatch: expected '$expected_version', found '$deb_version'"
    exit 1
  fi

  neutral_dir="$(mktemp -d "${TMPDIR:-/tmp}/yaze-release-install-smoke.XXXXXX")"
  cleanup_needed=true
  cleanup_install() {
    if [[ "$cleanup_needed" == true ]]; then
      sudo env DEBIAN_FRONTEND=noninteractive apt-get purge --yes yaze >/dev/null 2>&1 || true
    fi
    rm -rf "$neutral_dir"
  }
  trap cleanup_install EXIT

  sudo env DEBIAN_FRONTEND=noninteractive \
    apt-get install --yes --no-install-recommends "$package_path"

  if ! dpkg-query -W -f='${Status}' yaze 2>/dev/null | grep -qx 'install ok installed'; then
    echo "APT did not register yaze as installed"
    exit 1
  fi

  installed_files="$(dpkg-query -L yaze)"
  for required_path in /usr/bin/yaze /usr/bin/z3ed /usr/share/yaze/manifest.json; do
    if [[ ! -f "$required_path" ]]; then
      echo "Installed package is missing: $required_path"
      exit 1
    fi
    if ! grep -Fxq "$required_path" <<<"$installed_files"; then
      echo "Installed path is not owned by the yaze package: $required_path"
      exit 1
    fi
  done
  if [[ ! -d /usr/share/yaze/assets ]] || \
     [[ -z "$(find /usr/share/yaze/assets -type f -print -quit)" ]]; then
    echo "Installed assets directory is missing or empty"
    exit 1
  fi

  resolved_yaze="$(PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin command -v yaze)"
  resolved_z3ed="$(PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin command -v z3ed)"
  if [[ "$resolved_yaze" != "/usr/bin/yaze" || "$resolved_z3ed" != "/usr/bin/z3ed" ]]; then
    echo "Installed commands did not resolve through /usr/bin"
    exit 1
  fi

  check_dependencies /usr/bin/yaze
  check_dependencies /usr/bin/z3ed
  run_smoke_test "installed yaze --version" "$neutral_dir" /usr/bin/yaze --version "yaze $expected_version"
  run_smoke_test "installed z3ed --self-test" "$neutral_dir" /usr/bin/z3ed --self-test

  sudo env DEBIAN_FRONTEND=noninteractive apt-get purge --yes yaze
  cleanup_needed=false

  if dpkg-query -W yaze >/dev/null 2>&1; then
    echo "yaze remained in the package database after purge"
    exit 1
  fi
  for removed_path in /usr/bin/yaze /usr/bin/z3ed /usr/share/yaze /usr/share/doc/yaze; do
    if [[ -e "$removed_path" || -L "$removed_path" ]]; then
      echo "Package payload remained after purge: $removed_path"
      exit 1
    fi
  done

  echo "DEB install/uninstall smoke test passed: $package_path"
  exit 0
fi

extract_dir="$(mktemp -d "${TMPDIR:-/tmp}/yaze-release-smoke.XXXXXX")"
neutral_dir="$extract_dir/neutral"
mkdir -p "$neutral_dir"
cleanup_extract() {
  rm -rf "$extract_dir"
}
trap cleanup_extract EXIT

case "$package_path" in
  *.tar.gz|*.tgz)
    tar -xzf "$package_path" -C "$extract_dir"
    ;;
  *.deb)
    command -v dpkg-deb >/dev/null 2>&1 || {
      echo "dpkg-deb is required to smoke-test a DEB package"
      exit 1
    }
    dpkg-deb -x "$package_path" "$extract_dir"
    ;;
  *)
    echo "Unsupported Linux package: $package_path"
    exit 2
    ;;
esac

yaze_matches=()
while IFS= read -r -d '' path; do
  yaze_matches+=("$path")
done < <(find "$extract_dir" -type f -path '*/usr/bin/yaze' -print0)

if [[ ${#yaze_matches[@]} -ne 1 ]]; then
  echo "Expected exactly one usr/bin/yaze in the package, found ${#yaze_matches[@]}"
  exit 1
fi

yaze_binary="${yaze_matches[0]}"
payload_root="${yaze_binary%/usr/bin/yaze}"
z3ed_binary="$payload_root/usr/bin/z3ed"
assets_dir="$payload_root/usr/share/yaze/assets"

if [[ ! -x "$yaze_binary" ]]; then
  echo "Packaged yaze is not executable: $yaze_binary"
  exit 1
fi
if [[ ! -x "$z3ed_binary" ]]; then
  echo "Packaged z3ed is missing or not executable: $z3ed_binary"
  exit 1
fi
if [[ ! -d "$assets_dir" ]] || [[ -z "$(find "$assets_dir" -type f -print -quit)" ]]; then
  echo "Packaged assets directory is missing or empty: $assets_dir"
  exit 1
fi

echo "Checking packaged runtime dependencies..."
check_dependencies "$yaze_binary"
check_dependencies "$z3ed_binary"

echo "Smoke-testing packaged executables from a neutral working directory..."
run_smoke_test "yaze --version" "$neutral_dir" "$yaze_binary" --version "yaze $expected_version"
run_smoke_test "z3ed --self-test" "$neutral_dir" "$z3ed_binary" --self-test

echo "Linux package smoke test passed: $package_path"
