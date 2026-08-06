#!/usr/bin/env bash

set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  scripts/agents/test-installed-macos-quit.sh \
    --app /Applications/yaze.app \
    --project /path/to/Oracle-of-Secrets.yaze \
    --rom /path/to/oos168.sfc \
    [--cycles 10] [--room 152] [--evidence-dir /path/to/new-directory]

Launches the installed macOS application as a direct child, requests normal
Cocoa Quit, and requires a zero exit status with no new crash report, stale
process/status file, or project/ROM hash change.
EOF
}

die() {
  echo "error: $*" >&2
  exit 1
}

require_value() {
  [[ $# -ge 2 && -n "$2" ]] || die "missing value for $1"
}

APP=""
PROJECT=""
ROM=""
CYCLES=10
ROOM=152
EVIDENCE_DIR=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --app)
      require_value "$@"
      APP="$2"
      shift 2
      ;;
    --app=*) APP="${1#*=}"; shift ;;
    --project)
      require_value "$@"
      PROJECT="$2"
      shift 2
      ;;
    --project=*) PROJECT="${1#*=}"; shift ;;
    --rom)
      require_value "$@"
      ROM="$2"
      shift 2
      ;;
    --rom=*) ROM="${1#*=}"; shift ;;
    --cycles)
      require_value "$@"
      CYCLES="$2"
      shift 2
      ;;
    --cycles=*) CYCLES="${1#*=}"; shift ;;
    --room)
      require_value "$@"
      ROOM="$2"
      shift 2
      ;;
    --room=*) ROOM="${1#*=}"; shift ;;
    --evidence-dir)
      require_value "$@"
      EVIDENCE_DIR="$2"
      shift 2
      ;;
    --evidence-dir=*) EVIDENCE_DIR="${1#*=}"; shift ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      usage >&2
      die "unknown argument: $1"
      ;;
  esac
done

[[ "$(uname -s)" == "Darwin" ]] || die "this qualification requires macOS"
[[ -n "$APP" ]] || die "--app is required"
[[ -n "$PROJECT" ]] || die "--project is required"
[[ -n "$ROM" ]] || die "--rom is required"
[[ "$CYCLES" =~ ^[1-9][0-9]*$ ]] || die "--cycles must be a positive integer"
[[ "$ROOM" =~ ^[0-9]+$ ]] || die "--room must be a non-negative integer"

YAZE_BIN="$APP/Contents/MacOS/yaze"
INFO_PLIST="$APP/Contents/Info.plist"
[[ -x "$YAZE_BIN" ]] || die "Yaze executable not found: $YAZE_BIN"
[[ -f "$INFO_PLIST" ]] || die "Info.plist not found: $INFO_PLIST"
[[ -f "$PROJECT" ]] || die "project not found: $PROJECT"
[[ -f "$ROM" ]] || die "ROM not found: $ROM"

if [[ -z "$EVIDENCE_DIR" ]]; then
  EVIDENCE_DIR="$(mktemp -d "${TMPDIR:-/tmp}/yaze-macos-quit.XXXXXX")"
else
  [[ ! -e "$EVIDENCE_DIR" ]] || die "evidence directory already exists: $EVIDENCE_DIR"
  mkdir -p "$EVIDENCE_DIR"
fi

BUNDLE_ID="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$INFO_PLIST")"
[[ -n "$BUNDLE_ID" ]] || die "bundle identifier is empty"
YAZE_PROCESS_PATTERN='/[Yy]aze\.app/Contents/MacOS/yaze([[:space:]]|$)'

if pgrep -f "$YAZE_PROCESS_PATTERN" >/dev/null 2>&1; then
  die "another Yaze application instance is already running"
fi

sha256_file() {
  shasum -a 256 "$1" | awk '{print $1}'
}

PROJECT_SHA256="$(sha256_file "$PROJECT")"
ROM_SHA256="$(sha256_file "$ROM")"
printf '%s  %s\n%s  %s\n' \
  "$PROJECT_SHA256" "$PROJECT" "$ROM_SHA256" "$ROM" \
  >"$EVIDENCE_DIR/baseline.sha256"

CRASH_DIR="$HOME/Library/Logs/DiagnosticReports"
SUMMARY="$EVIDENCE_DIR/cycles.tsv"
printf 'cycle\tpid\texit_status\tproject_sha256\trom_sha256\tresult\n' >"$SUMMARY"

for cycle in $(seq 1 "$CYCLES"); do
  cycle_dir="$EVIDENCE_DIR/cycle-$(printf '%02d' "$cycle")"
  runtime_dir="$cycle_dir/runtime"
  app_data_dir="$cycle_dir/app-data"
  mkdir -p "$runtime_dir" "$app_data_dir"
  chmod 700 "$runtime_dir"
  marker="$cycle_dir/crash-marker"
  touch "$marker"

  XDG_RUNTIME_DIR="$runtime_dir" \
  YAZE_APP_DATA_DIR="$app_data_dir" \
    "$YAZE_BIN" \
      --rom_file="$PROJECT" \
      --editor=Dungeon \
      --room="$ROOM" \
      --startup_welcome=hide \
      --startup_dashboard=hide \
      --log_file="$cycle_dir/yaze.log" \
      --log_level=debug \
      --log_to_console \
      >"$cycle_dir/stdout.log" 2>"$cycle_dir/stderr.log" &
  pid=$!
  printf '%s\n' "$pid" >"$cycle_dir/pid"

  ready=false
  for _ in $(seq 1 120); do
    if ! kill -0 "$pid" 2>/dev/null; then
      break
    fi
    status_file="$runtime_dir/yaze-$pid.status"
    if [[ -f "$cycle_dir/yaze.log" && -f "$status_file" ]] &&
       grep -Eq "\"pid\"[[:space:]]*:[[:space:]]*$pid([,}])" \
         "$status_file" &&
       grep -q "RenderRoomGraphics.*Room ${ROOM}: Rendering graphics" \
         "$cycle_dir/yaze.log"; then
      ready=true
      break
    fi
    sleep 0.25
  done
  if [[ "$ready" != true ]]; then
    kill -TERM "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true
    die "cycle $cycle did not render room $ROOM; evidence: $cycle_dir"
  fi

  # The delegate intentionally returns NSTerminateCancel so Cocoa does not
  # call exit() re-entrantly; osascript can therefore report -128 even though
  # EditorManager accepted the Quit request. The child exit is authoritative.
  set +e
  osascript -e "tell application id \"$BUNDLE_ID\" to quit" \
    >"$cycle_dir/quit.stdout" 2>"$cycle_dir/quit.stderr"
  quit_request_status=$?
  set -e
  printf '%s\n' "$quit_request_status" >"$cycle_dir/quit-request-status"
  if [[ "$quit_request_status" -ne 0 ]] &&
     ! grep -q 'User canceled\. (-128)' "$cycle_dir/quit.stderr"; then
    kill -TERM "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true
    die "cycle $cycle did not deliver Cocoa Quit; evidence: $cycle_dir"
  fi

  timeout_marker="$cycle_dir/exit-timeout"
  (
    sleep 30
    if kill -0 "$pid" 2>/dev/null; then
      touch "$timeout_marker"
      kill -TERM "$pid" 2>/dev/null || true
      sleep 3
      kill -KILL "$pid" 2>/dev/null || true
    fi
  ) &
  watchdog_pid=$!

  set +e
  wait "$pid"
  exit_status=$?
  set -e
  kill "$watchdog_pid" 2>/dev/null || true
  wait "$watchdog_pid" 2>/dev/null || true
  printf '%s\n' "$exit_status" >"$cycle_dir/exit-status"

  # CrashReporter writes asynchronously after the process exits.
  sleep 3
  find "$CRASH_DIR" -maxdepth 1 -type f \
    \( -iname 'yaze-*.ips' -o -iname 'yaze-*.crash' \) \
    -newer "$marker" -print >"$cycle_dir/new-crash-reports.txt" 2>/dev/null || true

  project_after="$(sha256_file "$PROJECT")"
  rom_after="$(sha256_file "$ROM")"
  printf '%s  %s\n%s  %s\n' \
    "$project_after" "$PROJECT" "$rom_after" "$ROM" \
    >"$cycle_dir/after.sha256"

  stale_status=false
  [[ -e "$runtime_dir/yaze-$pid.status" ]] && stale_status=true
  [[ -e "/tmp/yaze-$pid.status" ]] && stale_status=true
  stale_socket=false
  if find "$runtime_dir" -type s -print -quit | grep -q .; then
    stale_socket=true
  fi
  normal_shutdown=true
  grep -q "Removing activity file:" "$cycle_dir/yaze.log" ||
    normal_shutdown=false
  grep -q "Destroying ImGui context" "$cycle_dir/yaze.log" ||
    normal_shutdown=false
  grep -q "Shutdown complete" "$cycle_dir/yaze.log" ||
    normal_shutdown=false

  result=pass
  [[ ! -e "$timeout_marker" ]] || result=fail
  [[ "$exit_status" -eq 0 ]] || result=fail
  [[ ! -s "$cycle_dir/new-crash-reports.txt" ]] || result=fail
  [[ "$stale_status" == false ]] || result=fail
  [[ "$stale_socket" == false ]] || result=fail
  [[ "$normal_shutdown" == true ]] || result=fail
  [[ "$project_after" == "$PROJECT_SHA256" ]] || result=fail
  [[ "$rom_after" == "$ROM_SHA256" ]] || result=fail
  if pgrep -f "$YAZE_PROCESS_PATTERN" >/dev/null 2>&1; then
    result=fail
  fi

  printf '%s\t%s\t%s\t%s\t%s\t%s\n' \
    "$cycle" "$pid" "$exit_status" "$project_after" "$rom_after" "$result" \
    >>"$SUMMARY"

  if [[ "$result" != pass ]]; then
    cat "$SUMMARY" >&2
    die "cycle $cycle failed shutdown qualification; evidence: $cycle_dir"
  fi
done

printf 'PASS: %s/%s normal Quit cycles\nEvidence: %s\n' \
  "$CYCLES" "$CYCLES" "$EVIDENCE_DIR"
