#!/usr/bin/env bash
set -euo pipefail

# Unified background agent script for YAZE (macOS launchd, Linux systemd)
# Subcommands:
#   install [--build-type X] [--use-inotify] [--ubuntu-bootstrap] [--enable-linger]
#   uninstall
#   run-once        # one-shot build & test
#   watch           # linux: inotify-based watch loop

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

BUILD_DIR_DEFAULT="${PROJECT_ROOT}/build"
BUILD_TYPE_DEFAULT="Debug"

usage() {
  cat <<EOF
Usage: $0 <subcommand> [options]

Subcommands:
  install            Install background agent
    --build-type X       CMake build type (default: ${BUILD_TYPE_DEFAULT})
    --use-inotify        Linux: use long-lived inotify watcher
    --ubuntu-bootstrap   Install Ubuntu deps via apt (sudo)
    --enable-linger      Enable user lingering (Linux)

  uninstall          Remove installed background agent

  run-once           One-shot build and test
    env: BUILD_DIR, BUILD_TYPE

  watch              Linux: inotify-based watch loop
    env: BUILD_DIR, BUILD_TYPE, INTERVAL_SECONDS (fallback)

Environment:
  BUILD_DIR   Build directory (default: ${BUILD_DIR_DEFAULT})
  BUILD_TYPE  CMake build type (default: ${BUILD_TYPE_DEFAULT})
EOF
}

build_and_test() {
  local build_dir="${BUILD_DIR:-${BUILD_DIR_DEFAULT}}"
  local build_type="${BUILD_TYPE:-${BUILD_TYPE_DEFAULT}}"
  local log_file="${LOG_FILE:-"${build_dir}/yaze_agent.log"}"

  mkdir -p "${build_dir}"
  {
    echo "==== $(date '+%Y-%m-%d %H:%M:%S') | yaze agent run (type=${build_type}) ===="
    echo "Project: ${PROJECT_ROOT}"

    cmake -S "${PROJECT_ROOT}" -B "${build_dir}" -DCMAKE_BUILD_TYPE="${build_type}"
    cmake --build "${build_dir}" --config "${build_type}" -j

    if [[ -x "${build_dir}/bin/yaze_test" ]]; then
      "${build_dir}/bin/yaze_test"
    else
      if command -v ctest >/dev/null 2>&1; then
        ctest --test-dir "${build_dir}" --output-on-failure
      else
        echo "ctest not found and test binary missing. Skipping tests." >&2
        exit 1
      fi
    fi
  } >>"${log_file}" 2>&1
}

sub_run_once() {
  build_and_test
}

sub_watch() {
  local build_dir="${BUILD_DIR:-${BUILD_DIR_DEFAULT}}"
  local build_type="${BUILD_TYPE:-${BUILD_TYPE_DEFAULT}}"
  local interval="${INTERVAL_SECONDS:-5}"
  mkdir -p "${build_dir}"
  if command -v inotifywait >/dev/null 2>&1; then
    echo "[agent] using inotifywait watcher"
    while true; do
      BUILD_DIR="${build_dir}" BUILD_TYPE="${build_type}" build_and_test || true
      inotifywait -r -e modify,create,delete,move \
        "${PROJECT_ROOT}/src" \
        "${PROJECT_ROOT}/test" \
        "${PROJECT_ROOT}/CMakeLists.txt" \
        "${PROJECT_ROOT}/src/CMakeLists.txt" >/dev/null 2>&1 || true
    done
  else
    echo "[agent] inotifywait not found; running periodic loop (${interval}s)"
    while true; do
      BUILD_DIR="${build_dir}" BUILD_TYPE="${build_type}" build_and_test || true
      sleep "${interval}"
    done
  fi
}

ensure_exec() {
  if [[ ! -x "$1" ]]; then
    chmod +x "$1"
  fi
}

ubuntu_bootstrap() {
  if command -v apt-get >/dev/null 2>&1; then
    sudo apt-get update
    sudo apt-get install -y inotify-tools build-essential cmake ninja-build pkg-config \
      libsdl2-dev libpng-dev libglew-dev libwavpack-dev libabsl-dev \
      libboost-all-dev libboost-python-dev python3-dev libpython3-dev
  else
    echo "apt-get not found; skipping Ubuntu bootstrap" >&2
  fi
}

enable_linger_linux() {
  if command -v loginctl >/dev/null 2>&1; then
    if sudo -n true 2>/dev/null; then
      sudo loginctl enable-linger "$USER" || true
    else
      echo "Note: enabling linger may require sudo: sudo loginctl enable-linger $USER" >&2
    fi
  fi
}

# Wrapper to run systemctl --user with a session bus in headless shells
systemctl_user() {
  # Only apply on Linux
  if [[ "$(uname -s)" != "Linux" ]]; then
    systemctl "$@"
    return
  fi
  local uid
  uid="$(id -u)"
  export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/${uid}}"
  export DBUS_SESSION_BUS_ADDRESS="${DBUS_SESSION_BUS_ADDRESS:-unix:path=${XDG_RUNTIME_DIR}/bus}"
  if [[ ! -S "${XDG_RUNTIME_DIR}/bus" ]]; then
    echo "[agent] Warning: user bus not found at ${XDG_RUNTIME_DIR}/bus. If this fails, run: sudo loginctl enable-linger $USER" >&2
  fi
  systemctl --user "$@"
}

is_systemd_available() {
  # True if systemd is PID 1 and systemctl exists and a user bus likely available
  if [[ ! -d /run/systemd/system ]]; then
    return 1
  fi
  if ! command -v systemctl >/dev/null 2>&1; then
    return 1
  fi
  return 0
}

start_userland_agent() {
  local build_type="${1}"
  local build_dir="${2}"
  local agent_home="$HOME/.local/share/yaze-agent"
  mkdir -p "${agent_home}"
  local log_file="${agent_home}/agent.log"
  nohup env BUILD_TYPE="${build_type}" BUILD_DIR="${build_dir}" "${SCRIPT_DIR}/agent.sh" watch >>"${log_file}" 2>&1 & echo $! > "${agent_home}/agent.pid"
  echo "Userland agent started (PID $(cat "${agent_home}/agent.pid")) - logs: ${log_file}"
}

stop_userland_agent() {
  local agent_home="$HOME/.local/share/yaze-agent"
  local pid_file="${agent_home}/agent.pid"
  if [[ -f "${pid_file}" ]]; then
    local pid
    pid="$(cat "${pid_file}")"
    if kill -0 "${pid}" >/dev/null 2>&1; then
      kill "${pid}" || true
    fi
    rm -f "${pid_file}"
    echo "Stopped userland agent"
  fi
}

install_macos() {
  local build_type="${1}"
  local build_dir="${2}"
  local label="com.yaze.watchtest"
  local plist_path="$HOME/Library/LaunchAgents/${label}.plist"

  mkdir -p "${build_dir}"

  cat >"$plist_path" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple Computer//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>Label</key>
  <string>${label}</string>
  <key>RunAtLoad</key>
  <true/>
  <key>ProgramArguments</key>
  <array>
    <string>/bin/zsh</string>
    <string>-lc</string>
    <string>"${SCRIPT_DIR}/agent.sh" run-once</string>
  </array>
  <key>WorkingDirectory</key>
  <string>${PROJECT_ROOT}</string>
  <key>WatchPaths</key>
  <array>
    <string>${PROJECT_ROOT}/src</string>
    <string>${PROJECT_ROOT}/test</string>
    <string>${PROJECT_ROOT}/CMakeLists.txt</string>
    <string>${PROJECT_ROOT}/src/CMakeLists.txt</string>
  </array>
  <key>StandardOutPath</key>
  <string>${build_dir}/yaze_agent.out.log</string>
  <key>StandardErrorPath</key>
  <string>${build_dir}/yaze_agent.err.log</string>
  <key>EnvironmentVariables</key>
  <dict>
    <key>BUILD_TYPE</key><string>${build_type}</string>
    <key>BUILD_DIR</key><string>${build_dir}</string>
  </dict>
</dict>
</plist>
PLIST

  launchctl unload -w "$plist_path" 2>/dev/null || true
  launchctl load -w "$plist_path"
  echo "LaunchAgent installed and loaded: ${plist_path}"
}

install_linux() {
  local build_type="${1}"
  local build_dir="${2}"
  local use_inotify="${3}"
  local systemd_dir="$HOME/.config/systemd/user"
  local service_name="yaze-watchtest.service"
  local path_name="yaze-watchtest.path"

  mkdir -p "${systemd_dir}" "${build_dir}"

  if ! is_systemd_available; then
    echo "[agent] systemd not available; installing userland background agent"
    start_userland_agent "${build_type}" "${build_dir}"
    return
  fi

  if [[ "${use_inotify}" == "1" ]]; then
    cat >"${systemd_dir}/yaze-watchtest-inotify.service" <<UNIT
[Unit]
Description=Yaze inotify watcher build-and-test

[Service]
Type=simple
Environment=BUILD_TYPE=${build_type}
Environment=BUILD_DIR=${build_dir}
WorkingDirectory=${PROJECT_ROOT}
ExecStart=/bin/bash -lc '"${SCRIPT_DIR}/agent.sh" watch'
Restart=always
RestartSec=2

[Install]
WantedBy=default.target
UNIT

    systemctl_user daemon-reload
    systemctl_user enable --now yaze-watchtest-inotify.service
    echo "systemd user service enabled: yaze-watchtest-inotify.service"
    return
  fi

  cat >"${systemd_dir}/${service_name}" <<UNIT
[Unit]
Description=Yaze build-and-test (one-shot)

[Service]
Type=oneshot
Environment=BUILD_TYPE=${build_type}
Environment=BUILD_DIR=${build_dir}
WorkingDirectory=${PROJECT_ROOT}
ExecStart=/bin/bash -lc '"${SCRIPT_DIR}/agent.sh" run-once'
UNIT

  cat >"${systemd_dir}/${path_name}" <<UNIT
[Unit]
Description=Watch Yaze src/test for changes

[Path]
PathModified=${PROJECT_ROOT}/src
PathModified=${PROJECT_ROOT}/test
PathModified=${PROJECT_ROOT}/CMakeLists.txt
PathModified=${PROJECT_ROOT}/src/CMakeLists.txt
Unit=${service_name}

[Install]
WantedBy=default.target
UNIT

  systemctl_user daemon-reload
  systemctl_user enable --now "$path_name"
  echo "systemd user path unit enabled: ${path_name}"
  systemctl_user start "$service_name" || true
}

sub_install() {
  local build_type="${BUILD_TYPE:-${BUILD_TYPE_DEFAULT}}"
  local build_dir="${BUILD_DIR:-${BUILD_DIR_DEFAULT}}"
  local use_inotify=0
  local do_bootstrap=0
  local do_linger=0
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --build-type) build_type="$2"; shift 2 ;;
      --use-inotify) use_inotify=1; shift ;;
      --ubuntu-bootstrap) do_bootstrap=1; shift ;;
      --enable-linger) do_linger=1; shift ;;
      -h|--help) usage; exit 0 ;;
      *) echo "Unknown option for install: $1" >&2; usage; exit 1 ;;
    esac
  done

  case "$(uname -s)" in
    Darwin)
      install_macos "${build_type}" "${build_dir}" ;;
    Linux)
      if (( do_bootstrap == 1 )); then ubuntu_bootstrap; fi
      if (( do_linger == 1 )); then enable_linger_linux; fi
      install_linux "${build_type}" "${build_dir}" "${use_inotify}" ;;
    *)
      echo "Unsupported platform" >&2; exit 1 ;;
  esac
}

sub_uninstall() {
  case "$(uname -s)" in
    Darwin)
      local label="com.yaze.watchtest"
      local plist_path="$HOME/Library/LaunchAgents/${label}.plist"
      launchctl unload -w "$plist_path" 2>/dev/null || true
      rm -f "$plist_path"
      echo "Removed LaunchAgent ${label}"
      ;;
    Linux)
      local systemd_dir="$HOME/.config/systemd/user"
      if is_systemd_available; then
      systemctl_user stop yaze-watchtest.path 2>/dev/null || true
      systemctl_user disable yaze-watchtest.path 2>/dev/null || true
      systemctl_user stop yaze-watchtest.service 2>/dev/null || true
      systemctl_user stop yaze-watchtest-inotify.service 2>/dev/null || true
      systemctl_user disable yaze-watchtest-inotify.service 2>/dev/null || true
      rm -f "${systemd_dir}/yaze-watchtest.service" "${systemd_dir}/yaze-watchtest.path" "${systemd_dir}/yaze-watchtest-inotify.service"
      systemctl_user daemon-reload || true
      echo "Removed systemd user units"
      fi
      stop_userland_agent
      ;;
    *) echo "Unsupported platform" >&2; exit 1 ;;
  esac
}

main() {
  ensure_exec "$0"
  local subcmd="${1:-}"; shift || true
  case "${subcmd}" in
    install) sub_install "$@" ;;
    uninstall) sub_uninstall ;;
    run-once) sub_run_once ;;
    watch) sub_watch ;;
    -h|--help|help|"") usage ;;
    *) echo "Unknown subcommand: ${subcmd}" >&2; usage; exit 1 ;;
  esac
}

main "$@"


