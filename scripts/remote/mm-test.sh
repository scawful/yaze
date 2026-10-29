#!/usr/bin/env bash
# mm-test.sh - build and run yaze tests on the Windows build box over SSH.
#
# Usage: scripts/remote/mm-test.sh <branch|commit> [options]
#   --filter <gtest filter>  run yaze_test_unit directly with --gtest_filter
#   --gtest-args <args>      extra raw flags appended to a direct yaze_test_unit
#                            run, e.g. '--gtest_shuffle --gtest_random_seed=1
#                            --gtest_repeat=5'; implies --filter '*' if --filter
#                            is not also given, so the whole binary runs as one
#                            process (needed to reproduce test-order-dependent
#                            failures, which per-fixture ctest shards do not hit)
#   --full                   run ctest label "stable" (unit + integration, as CI does)
#   --target <cmake target>  build this target instead (repeatable); without
#                            --filter/--full this is a build-only run
#   --jobs <n>               build/test parallelism (default: sized to free RAM)
#   --clean                  delete the remote build dir first (fresh configure)
#   --no-ccache              compile without ccache (cold timings, cache debugging)
#   --preset <name>          CMake configure preset (default: ci-windows)
#   --out <dir>              local results dir (default: build/remote-mm/<slug>)
# Default (no options): build yaze_test_unit and run ctest label "unit".
#
# Works for local-only branches: the commits the PC lacks travel as a git bundle.
# Env: MM_HOST (ssh alias; forces one box), MM_HOSTS (boxes tried in order when
# MM_HOST is unset, default "mm": first ready box with a free build lock, else
# wait on the first reachable), MM_REMOTE_ROOT (runner dir relative to the
# remote home, default src/yaze-remote), MM_MAX_WORKTREES (review threshold,
# default 8; report only, never deletes worktrees, stamps or job evidence),
# MM_LOCK_WAIT_MIN (default 120).
# Exit: 0 pass, 1 test failures, 2 configure/build failure, 3 setup/usage error.
# See scripts/remote/README.md.
set -euo pipefail

usage() { sed -n '2,29p' "$0" | sed 's/^# \{0,1\}//'; exit "${1:-3}"; }
die() { echo "mm-test: $*" >&2; exit 3; }

host="${MM_HOST:-}"
remote_root="${MM_REMOTE_ROOT:-src/yaze-remote}"
preset="ci-windows"
ref="" filter="" gtest_args="" full=0 clean=0 ccache=1 jobs="" out_dir="" targets=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    -h|--help) usage 0 ;;
    --filter) filter="${2:?--filter needs a value}"; shift 2 ;;
    --gtest-args) gtest_args="${2:?--gtest-args needs a value}"; shift 2 ;;
    --full) full=1; shift ;;
    --target) targets+=("${2:?--target needs a value}"); shift 2 ;;
    --jobs) jobs="${2:?--jobs needs a value}"; shift 2 ;;
    --clean) clean=1; shift ;;
    --no-ccache) ccache=0; shift ;;
    --preset) preset="${2:?--preset needs a value}"; shift 2 ;;
    --out) out_dir="${2:?--out needs a value}"; shift 2 ;;
    -*) die "unknown option $1 (see --help)" ;;
    *) [[ -z "$ref" ]] || die "only one ref allowed"; ref="$1"; shift ;;
  esac
done
[[ -n "$ref" ]] || usage 3
[[ "$filter" =~ ^[A-Za-z0-9_*?:./-]*$ ]] || die "filter may only use [A-Za-z0-9_*?:./-]"
gtest_args_re='^[A-Za-z0-9_=,.:/ -]*$'
[[ "$gtest_args" =~ $gtest_args_re ]] || die "gtest-args may only use [A-Za-z0-9_=,.:/ -]"
[[ "$jobs" =~ ^[0-9]*$ ]] || die "--jobs must be a number"

# Prints FREE or BUSY for a box's build lock; UNPREPARED if build tools are missing;
# fails if the box is unreachable.
lock_state() {
  local script="\$t = Join-Path \$env:USERPROFILE 'tools'
if (-not ((Test-Path \"\$t\\ninja\\ninja.exe\") -and (Test-Path \"\$t\\ccache\\ccache.exe\") -and (Get-Command cmake -ErrorAction SilentlyContinue))) { 'UNPREPARED'; exit }
\$p = Join-Path \$env:USERPROFILE '${remote_root//\//\\}\\build.lock'
if (-not (Test-Path \$p)) { 'FREE'; exit }
try { [IO.File]::Open(\$p, 'Open', 'ReadWrite', 'None').Close(); 'FREE' } catch { 'BUSY' }"
  ssh -o BatchMode=yes -o LogLevel=ERROR -o ConnectTimeout=5 "$1" \
    "pwsh -NoProfile -EncodedCommand $(printf '%s' "$script" | iconv -t UTF-16LE | base64 | tr -d '\n')" \
    | tr -d '\r'
}
if [[ -z "$host" ]]; then
  fallback=""
  for candidate in ${MM_HOSTS:-mm}; do
    state="$(lock_state "$candidate" 2>/dev/null)" || { echo "[mac] $candidate unreachable"; continue; }
    if [[ "$state" == UNPREPARED ]]; then echo "[mac] $candidate missing build tools (unprepared)"; continue; fi
    if [[ "$state" == FREE ]]; then host="$candidate"; break; fi
    echo "[mac] $candidate build lock busy"
    fallback="${fallback:-$candidate}"
  done
  host="${host:-$fallback}"
  [[ -n "$host" ]] || die "no ready build box reachable (${MM_HOSTS:-mm})"
fi

repo_root="$(git rev-parse --show-toplevel)" || die "run inside a yaze checkout"
sha="$(git rev-parse --verify --quiet "${ref}^{commit}")" || die "unknown ref: $ref"
if git show-ref --verify --quiet "refs/heads/$ref"; then name="$ref"
elif git show-ref --verify --quiet "refs/remotes/$ref"; then name="${ref#*/}"
else name="rev-${sha:0:12}"; fi
slug="$(printf '%s' "$name" | tr -c 'A-Za-z0-9._-' '-' | cut -c1-48)"

mode=unit
if [[ -n "$filter" || -n "$gtest_args" ]]; then mode=filter
elif [[ $full -eq 1 ]]; then mode=stable
elif [[ ${#targets[@]} -gt 0 ]]; then mode=build; fi
case "$mode" in
  unit|filter) targets+=(yaze_test_unit) ;;
  stable) targets+=(yaze_test_unit yaze_test_integration) ;;
esac

job_id="$(date +%Y%m%d-%H%M%S)-$$"
remote_job="$remote_root/jobs/$job_id"
remote_job_win="${remote_job//\//\\}"
tmp="$(mktemp -d)"
tmp_ref="refs/mm-test/$job_id"
cleanup() { git update-ref -d "$tmp_ref" 2>/dev/null || true; rm -rf "$tmp"; }
trap cleanup EXIT

# Windows OpenSSH + cmd.exe: strip CR/NUL, keep output streaming.
mm() { ssh -o BatchMode=yes -o LogLevel=ERROR "$host" "$@" | perl -pe 'BEGIN{$|=1} s/[\0\r]//g'; }

# 1. Ask the PC which commits it has, so the bundle carries only what is missing.
tips="$(mm "mkdir $remote_job_win 2>nul & git -C src/yaze for-each-ref --format=%(objectname) refs/heads refs/remotes refs/mm 2>nul & exit 0")" \
  || die "ssh $host failed"
have=()
while read -r obj type; do
  [[ "$type" == commit ]] && have+=("$obj")
done < <(grep -E '^[0-9a-f]{40}$' <<<"$tips" | sort -u | git cat-file --batch-check='%(objectname) %(objecttype)' || true)

bundle=""
missing="$(git rev-list --count "$sha" --not ${have[@]+"${have[@]}"} --)"
if [[ "$missing" -gt 0 ]]; then
  git update-ref "$tmp_ref" "$sha"
  git bundle create --quiet "$tmp/mm.bundle" "$tmp_ref" --not ${have[@]+"${have[@]}"}
  bundle="mm.bundle"
  echo "[mac] bundle: $missing commit(s), $(du -h "$tmp/mm.bundle" | cut -f1 | tr -d ' ')"
fi

origin="$(git remote get-url origin 2>/dev/null || true)"
[[ "$origin" =~ ^https:// ]] || origin=""
cat > "$tmp/job.txt" <<EOF
ref=$ref
sha=$sha
slug=$slug
mode=$mode
preset=$preset
config=RelWithDebInfo
targets=${targets[*]}
filter=$filter
gtest_args=$gtest_args
jobs=$jobs
clean=$clean
ccache=$ccache
bundle=$bundle
bundle_ref=${bundle:+$tmp_ref}
origin=$origin
max_worktrees=${MM_MAX_WORKTREES:-8}
lock_wait_min=${MM_LOCK_WAIT_MIN:-120}
who=$(whoami)@$(hostname -s)
EOF
files=("$(dirname "$0")/mm-build.ps1" "$tmp/job.txt")
[[ -n "$bundle" ]] && files+=("$tmp/mm.bundle")
scp -q -o BatchMode=yes -o LogLevel=ERROR "${files[@]}" "$host:$remote_job/" || die "scp to $host failed"

# 2. Sync, build and test on the PC (holds the PC-side lock).
echo "[mac] $ref -> $slug @ ${sha:0:12} on $host (mode=$mode, targets=${targets[*]})"
start=$SECONDS
set +e
mm "pwsh -NoProfile -ExecutionPolicy Bypass -File $remote_job_win\\mm-build.ps1 -JobDir $remote_job_win"
rc=$?
set -e
[[ $rc -eq 255 ]] && die "ssh $host dropped (exit 255)"

# 3. Copy results back and summarize.
out_dir="${out_dir:-$repo_root/build/remote-mm/$slug}"
rm -rf "$out_dir"; mkdir -p "$out_dir"
scp -q -r -o BatchMode=yes -o LogLevel=ERROR "$host:$remote_job/out/*" "$out_dir/" 2>/dev/null || true
if [[ -s "$out_dir/junit.xml" ]]; then
  set +e
  python3 - "$out_dir/junit.xml" <<'PY'
import sys
import xml.etree.ElementTree as ET

root = ET.parse(sys.argv[1]).getroot()
total = failed = skipped = 0
failures = []
for case in root.iter("testcase"):
    total += 1
    name, cls = case.get("name", ""), case.get("classname", "")
    full = name if not cls or cls == name or name.startswith(cls) else f"{cls}.{name}"
    if case.find("failure") is not None or case.find("error") is not None or case.get("status") == "fail":
        failed += 1
        failures.append(full)
    elif case.find("skipped") is not None or case.get("status") in ("notrun", "disabled") \
            or case.get("result") == "skipped":
        skipped += 1
print(f"[mac] tests: {total - failed - skipped} passed, {failed} failed, {skipped} skipped (of {total})")
for f in failures[:40]:
    print(f"[mac]   FAILED {f}")
if len(failures) > 40:
    print(f"[mac]   ... {len(failures) - 40} more")
passed = total - failed - skipped
if total and passed == 0:
    print("[mac] ERROR: no test actually ran (all skipped); see test.log")
    sys.exit(1)
if skipped * 4 > total:
    print(f"[mac] WARNING: {skipped} of {total} tests skipped; see test.log")
PY
  py_rc=$?
  set -e
  [[ $rc -eq 0 && $py_rc -ne 0 ]] && rc=1
fi
echo "[mac] results: $out_dir (summary.txt, build.log, test.log, junit.xml)"
echo "[mac] $( [[ $rc -eq 0 ]] && echo PASS || echo "FAIL (exit $rc)" ) in $((SECONDS - start))s"
exit "$rc"
