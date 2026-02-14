#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

# Optional local overrides (gitignored).
SIGNING_ENV="${ROOT_DIR}/scripts/signing.env"
if [[ -f "${SIGNING_ENV}" ]]; then
  # shellcheck disable=SC1090
  source "${SIGNING_ENV}"
fi

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[info] $1${NC}"; }
log_ok() { echo -e "${GREEN}[ok] $1${NC}"; }
log_warn() { echo -e "${YELLOW}[warn] $1${NC}"; }
log_err() { echo -e "${RED}[err] $1${NC}"; }

usage() {
  cat <<'EOF'
Usage:
  scripts/dev/ios-ipad-workflow.sh list
  scripts/dev/ios-ipad-workflow.sh deploy [options]
  scripts/dev/ios-ipad-workflow.sh instant [options]
  scripts/dev/ios-ipad-workflow.sh redeploy [options]

Commands:
  list      Show currently paired+available iPad device names.
  deploy    Build once, then install/launch on one or more iPads.
  instant   Build only when iOS sources changed, then install/launch.
  redeploy  Skip build, install/launch last built .app.

Options:
  --preset <name>         xcodebuild-ios preset (default: ios-debug)
  --devices "<a,b,c>"     Comma-separated device names
  --all                   Target all paired+available iPads
  --skip-build            Skip build and deploy existing app bundle
  --force-build           Force build in instant mode (ignore cache)
  --retries <n>           Install retries per device (default: 2)
  --no-launch             Install only; do not auto-launch app
  --bundle-id <id>        Bundle identifier for launch
  --help                  Show this help

Selection priority:
  1) --devices
  2) --all
  3) YAZE_IOS_DEPLOY_DEVICES (comma-separated)
  4) YAZE_IOS_DEVICE
  5) "Baby Pad"

Examples:
  scripts/dev/ios-ipad-workflow.sh list
  scripts/dev/ios-ipad-workflow.sh deploy --all
  scripts/dev/ios-ipad-workflow.sh instant --all
  scripts/dev/ios-ipad-workflow.sh deploy --devices "Baby Pad,iPadothée Chalamet"
  scripts/dev/ios-ipad-workflow.sh redeploy --devices "Baby Pad"
EOF
}

require_devicectl() {
  if ! xcrun -f devicectl >/dev/null 2>&1; then
    log_err "xcrun devicectl not found; install/update Xcode command line tools."
    exit 1
  fi
}

list_available_ipads() {
  require_devicectl
  xcrun devicectl list devices \
    | awk -F '  +' 'NR>2 && $4 ~ /available \(paired\)/ && $5 ~ /iPad/ {print $1}'
}

print_available_ipads() {
  local devices
  devices="$(list_available_ipads || true)"
  if [[ -z "${devices}" ]]; then
    log_warn "No paired+available iPads detected."
    return 0
  fi
  printf '%s\n' "${devices}"
}

resolve_products_dir() {
  local preset="$1"
  local config="Debug"
  if [[ "${preset}" == *release* ]]; then
    config="Release"
  fi
  echo "${ROOT_DIR}/build/xcode/derived/yaze_ios-${preset}/Build/Products/${config}-iphoneos"
}

resolve_device_app_path() {
  local products_dir="$1"
  local preferred_path="${products_dir}/yaze_ios.app"
  if [[ -d "${preferred_path}" ]]; then
    printf '%s\n' "${preferred_path}"
    return 0
  fi

  mapfile -t app_bundles < <(find "${products_dir}" -maxdepth 1 -type d -name "*.app" | sort)
  if [[ ${#app_bundles[@]} -eq 0 ]]; then
    return 1
  fi

  for app in "${app_bundles[@]}"; do
    if [[ "$(basename "${app}")" == "yaze.app" ]]; then
      printf '%s\n' "${app}"
      return 0
    fi
  done

  if [[ ${#app_bundles[@]} -eq 1 ]]; then
    printf '%s\n' "${app_bundles[0]}"
    return 0
  fi

  return 1
}

hash_text() {
  if command -v shasum >/dev/null 2>&1; then
    shasum -a 256 | awk '{print $1}'
    return 0
  fi
  if command -v openssl >/dev/null 2>&1; then
    openssl dgst -sha256 | awk '{print $NF}'
    return 0
  fi
  return 1
}

compute_ios_build_fingerprint() {
  if ! command -v git >/dev/null 2>&1; then
    return 1
  fi
  if ! git -C "${ROOT_DIR}" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    return 1
  fi

  (
    cd "${ROOT_DIR}"
    {
      git rev-parse HEAD 2>/dev/null || true
      git status --porcelain --untracked-files=all -- \
        CMakeLists.txt \
        cmake \
        src \
        scripts/build-ios.sh \
        scripts/xcodebuild-ios.sh \
        scripts/dev/ios-ipad-workflow.sh || true
    } | hash_text
  )
}

fingerprint_file_for_preset() {
  local preset="$1"
  echo "${ROOT_DIR}/build/xcode/derived/yaze_ios-${preset}/.ios-ipad-build-fingerprint"
}

read_cached_fingerprint() {
  local file="$1"
  if [[ -f "${file}" ]]; then
    tr -d '[:space:]' < "${file}"
  fi
}

write_cached_fingerprint() {
  local file="$1"
  local fingerprint="$2"
  mkdir -p "$(dirname "${file}")"
  printf '%s\n' "${fingerprint}" > "${file}"
}

split_csv() {
  local csv="$1"
  local -n dest_ref="$2"
  dest_ref=()
  if [[ -z "${csv}" ]]; then
    return 0
  fi
  IFS=',' read -r -a dest_ref <<<"${csv}"
  for i in "${!dest_ref[@]}"; do
    # Trim leading/trailing ASCII spaces
    dest_ref[$i]="$(echo "${dest_ref[$i]}" | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//')"
  done
}

resolve_target_devices() {
  local all_flag="$1"
  local devices_csv="$2"
  local -n devices_ref="$3"
  devices_ref=()

  if [[ -n "${devices_csv}" ]]; then
    split_csv "${devices_csv}" devices_ref
    return 0
  fi

  if [[ "${all_flag}" == "true" ]]; then
    mapfile -t devices_ref < <(list_available_ipads)
    return 0
  fi

  if [[ -n "${YAZE_IOS_DEPLOY_DEVICES:-}" ]]; then
    split_csv "${YAZE_IOS_DEPLOY_DEVICES}" devices_ref
    return 0
  fi

  devices_ref=("${YAZE_IOS_DEVICE:-Baby Pad}")
}

deploy_to_devices() {
  local app_path="$1"
  local bundle_id="$2"
  local launch_after="$3"
  local retries="$4"
  shift 4
  local devices=("$@")

  local fail_count=0
  local ok_count=0

  for device in "${devices[@]}"; do
    if [[ -z "${device}" ]]; then
      continue
    fi

    local attempt=1
    local installed=false
    while [[ ${attempt} -le ${retries} ]]; do
      log_info "Installing on: ${device} (attempt ${attempt}/${retries})"
      local install_log
      install_log="$(mktemp)"
      if xcrun devicectl device install app --device "${device}" "${app_path}" >"${install_log}" 2>&1; then
        cat "${install_log}"
        rm -f "${install_log}"
        installed=true
        break
      fi

      cat "${install_log}"
      if grep -q "CoreDeviceError error 3002" "${install_log}" || \
         grep -q "installcoordination_proxy" "${install_log}"; then
        log_warn "Install coordination failed. Ensure device is unlocked, trusted, and Developer Mode is enabled."
      fi
      rm -f "${install_log}"

      if [[ ${attempt} -lt ${retries} ]]; then
        sleep 2
      fi
      ((attempt += 1))
    done

    if [[ "${installed}" == "true" ]]; then
      ((ok_count += 1))
      if [[ "${launch_after}" == "true" ]]; then
        log_info "Launching on: ${device}"
        if ! xcrun devicectl device process launch --device "${device}" --terminate-existing "${bundle_id}" >/dev/null 2>&1; then
          log_warn "Launch failed on '${device}' (install succeeded)"
        fi
      fi
    else
      ((fail_count += 1))
      log_err "Install failed on '${device}'"
    fi
  done

  if [[ ${fail_count} -gt 0 ]]; then
    log_err "Deployment complete with failures (${ok_count} succeeded, ${fail_count} failed)"
    return 1
  fi

  log_ok "Deployment complete (${ok_count} device(s))"
}

main() {
  local command="${1:-deploy}"
  shift || true

    local preset="ios-debug"
    local devices_csv=""
    local all_devices=false
    local skip_build=false
    local force_build=false
    local instant_mode=false
    local retries="${YAZE_IOS_DEPLOY_RETRIES:-2}"
    local launch_after=true
    local bundle_id="${YAZE_IOS_BUNDLE_ID:-org.halext.yaze-ios}"

  if [[ "${command}" == "instant" ]]; then
    instant_mode=true
    command="deploy"
  fi

  if [[ "${command}" == "redeploy" ]]; then
    skip_build=true
    command="deploy"
  fi

  case "${command}" in
    list|deploy) ;;
    -h|--help|help) usage; exit 0 ;;
    *)
      log_err "Unknown command: ${command}"
      usage
      exit 2
      ;;
  esac

  while [[ $# -gt 0 ]]; do
    case "$1" in
      --preset)
        preset="$2"; shift 2 ;;
      --devices)
        devices_csv="$2"; shift 2 ;;
      --all)
        all_devices=true; shift ;;
      --skip-build)
        skip_build=true; shift ;;
      --force-build)
        force_build=true; shift ;;
      --retries)
        retries="$2"; shift 2 ;;
      --no-launch)
        launch_after=false; shift ;;
      --bundle-id)
        bundle_id="$2"; shift 2 ;;
      --help|-h)
        usage; exit 0 ;;
      *)
        log_err "Unknown option: $1"
        usage
        exit 2
        ;;
    esac
  done

  if [[ "${command}" == "list" ]]; then
    print_available_ipads
    exit 0
  fi

  require_devicectl

  local devices=()
  resolve_target_devices "${all_devices}" "${devices_csv}" devices
  if [[ ${#devices[@]} -eq 0 ]]; then
    log_err "No target devices resolved."
    exit 1
  fi

  local app_products_dir
  app_products_dir="$(resolve_products_dir "${preset}")"

  local build_fingerprint=""
  local fingerprint_file=""
  if [[ "${instant_mode}" == "true" ]]; then
    fingerprint_file="$(fingerprint_file_for_preset "${preset}")"
    if [[ "${force_build}" == "true" ]]; then
      log_info "Instant mode: force-build enabled."
    elif [[ "${skip_build}" == "false" ]]; then
      build_fingerprint="$(compute_ios_build_fingerprint || true)"
      if [[ -z "${build_fingerprint}" ]]; then
        log_warn "Instant mode: fingerprint unavailable; running build."
      else
        local cached_fingerprint=""
        cached_fingerprint="$(read_cached_fingerprint "${fingerprint_file}")"
        if [[ -n "${cached_fingerprint}" && \
              "${cached_fingerprint}" == "${build_fingerprint}" ]] && \
              resolve_device_app_path "${app_products_dir}" >/dev/null 2>&1; then
          skip_build=true
          log_info "Instant mode: cache hit, reusing existing app bundle."
        else
          log_info "Instant mode: cache miss, building app."
        fi
      fi
    fi
  fi

  if [[ "${skip_build}" == "false" ]]; then
    log_info "Building once with preset '${preset}'"
    "${ROOT_DIR}/scripts/xcodebuild-ios.sh" "${preset}" build
    if [[ "${instant_mode}" == "true" ]]; then
      if [[ -z "${build_fingerprint}" ]]; then
        build_fingerprint="$(compute_ios_build_fingerprint || true)"
      fi
      if [[ -n "${build_fingerprint}" ]]; then
        write_cached_fingerprint "${fingerprint_file}" "${build_fingerprint}"
      fi
    fi
  else
    log_info "Skipping build (existing app bundle)"
  fi

  local app_path
  if ! app_path="$(resolve_device_app_path "${app_products_dir}")"; then
    log_err "Could not locate built .app in: ${app_products_dir}"
    log_err "Run without --skip-build to force a fresh build."
    exit 1
  fi

  log_info "Using app bundle: ${app_path}"
  log_info "Targets: ${devices[*]}"
  deploy_to_devices "${app_path}" "${bundle_id}" "${launch_after}" "${retries}" "${devices[@]}"
}

main "$@"
