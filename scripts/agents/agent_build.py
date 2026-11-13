#!/usr/bin/env python3
"""Build selected targets quietly with a preset, sized by the host build mode."""

import argparse
import json
import os
from pathlib import Path
import platform as host_platform
import re
import shlex
import shutil
import signal
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
# Desk uses the host's cores without a wrapper-imposed priority reduction.
# Laptop keeps the explicit restricted policy. Never alter a running build.
HOST_JOBS = max(1, os.cpu_count() or 1)
BUILD_MODES = {"desk": (HOST_JOBS, HOST_JOBS, []), "laptop": (2, 4, ["-b"])}
# Mac builds yield to the Windows build box when the Mac is short on memory
# or disk (AGENT_STORAGE_HYGIENE: 30 GiB free before allocating a new tree).
MAC_MAX_SWAP_FRACTION = 0.75
MAC_MIN_FREE_GIB = 30


class BuildError(Exception):
    pass


class BuildDirectoryLock:
    """One configure/build writer per cache; released by the OS on exit."""

    def __init__(self, directory):
        self.path = directory.parent / ("." + directory.name + ".agent-build.lock")
        self.stream = None

    def __enter__(self):
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self.stream = self.path.open("a+")
        try:
            if os.name == "nt":
                import msvcrt
                self.stream.seek(0)
                if not self.stream.read(1):
                    self.stream.write(" ")
                    self.stream.flush()
                self.stream.seek(0)
                msvcrt.locking(self.stream.fileno(), msvcrt.LK_NBLCK, 1)
            else:
                import fcntl
                fcntl.flock(self.stream.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError as error:
            self.stream.seek(0)
            owner = self.stream.read().strip()
            self.stream.close()
            self.stream = None
            raise BuildError(f"Build cache is already in use ({owner}): {self.path}") from error
        self.stream.seek(0)
        self.stream.truncate()
        self.stream.write(f"pid={os.getpid()}")
        self.stream.flush()
        return self

    def __exit__(self, *_args):
        self.stream.close()


class Cancellation:
    def __init__(self):
        self.received = None
        self.child = None
        self.old_handlers = {}

    def handle(self, signum, _frame):
        self.received = self.received or signum
        if self.child is not None:
            self.send_group(signum)

    def send_group(self, signum):
        try:
            if os.name == "nt":
                if signum == signal.SIGINT:
                    self.child.send_signal(signal.CTRL_BREAK_EVENT)
                else:
                    self.child.terminate()
            else:
                os.killpg(self.child.pid, signum)
        except ProcessLookupError:
            pass
        except PermissionError:
            # Darwin can report EPERM while an exiting group disappears.
            # Confirm that no executable members remain; never assume it free.
            if self.group_alive():
                raise

    def group_alive(self):
        result = subprocess.run(["ps", "-axo", "pgid=,stat="],
                                capture_output=True, text=True, check=True)
        for line in result.stdout.splitlines():
            fields = line.split()
            if len(fields) != 2 or not fields[0].isdigit():
                raise BuildError("Cannot verify child process-group exit")
            if int(fields[0]) == self.child.pid and not fields[1].startswith("Z"):
                return True
        return False

    def __enter__(self):
        for signum in (signal.SIGINT, signal.SIGTERM):
            self.old_handlers[signum] = signal.signal(signum, self.handle)
        return self

    def __exit__(self, *_args):
        for signum, handler in self.old_handlers.items():
            signal.signal(signum, handler)

    def check(self):
        if self.received:
            raise BuildError(f"Build cancelled by signal {self.received}")

    def run(self, command, root, env):
        self.check()
        print("Running: " + shlex.join(command), flush=True)
        options = ({"creationflags": subprocess.CREATE_NEW_PROCESS_GROUP}
                   if os.name == "nt" else {"start_new_session": True})
        self.child = subprocess.Popen(command, cwd=root, env=env, **options)
        cancelled_at = None
        termination_sent = False
        kill_sent = False
        finished = False
        try:
            while True:
                result = self.child.poll()
                # The leader waits for ordinary compiler children. Only after
                # it exits do we inspect remaining group members (including a
                # child whose parent exited early), avoiding polling ps during
                # a normal long build. Zombies have already terminated.
                group_alive = result is None or (os.name != "nt" and self.group_alive())
                if result is not None and not group_alive:
                    finished = True
                    break
                if self.received:
                    if cancelled_at is None:
                        cancelled_at = time.monotonic()
                        self.send_group(self.received)
                    elapsed = time.monotonic() - cancelled_at
                    # Escalate only our newly created process group. The helper
                    # returns only after its leader AND descendants have exited.
                    if elapsed >= 5 and not termination_sent:
                        self.send_group(signal.SIGTERM)
                        termination_sent = True
                    if elapsed >= 10 and not kill_sent:
                        if os.name == "nt":
                            self.child.kill()
                        else:
                            self.send_group(signal.SIGKILL)
                        kill_sent = True
                time.sleep(0.1)
            self.check()
            return result
        finally:
            if finished:
                self.child = None


def presets(root):
    result = {"configurePresets": {}, "buildPresets": {}}
    visiting, loaded = set(), set()

    def load(path):
        path = path.resolve()
        if path in visiting:
            raise BuildError(f"Preset include cycle: {path}")
        if path in loaded:
            return
        visiting.add(path)
        value = json.loads(path.read_text())
        for include in value.get("include", []):
            if not isinstance(include, str) or "$" in include:
                raise BuildError("Unsupported preset include; use a literal path")
            load(path.parent / include)
        for kind in result:
            for preset in value.get(kind, []):
                name = preset["name"]
                if name in result[kind]:
                    raise BuildError(f"Duplicate {kind} name: {name}")
                result[kind][name] = preset
        visiting.remove(path)
        loaded.add(path)

    load(root / "CMakePresets.json")
    if (root / "CMakeUserPresets.json").exists():
        load(root / "CMakeUserPresets.json")
    return result


def inherit(entries, name, visiting=()):
    if name in visiting or name not in entries:
        raise BuildError(f"Missing or cyclic preset: {name}")
    preset = entries[name]
    parents = preset.get("inherits", [])
    if isinstance(parents, str):
        parents = [parents]
    merged = {}
    # First inherited parent wins conflicts; dictionaries merge by key.
    for item in [inherit(entries, p, (*visiting, name)) for p in reversed(parents)] + [preset]:
        for key, value in item.items():
            if isinstance(value, dict) and isinstance(merged.get(key), dict):
                merged[key] = {**merged[key], **value}
            else:
                merged[key] = value
    return merged


def preset_environment(parent, updates, values):
    resolved = {}

    def expand(value, visiting=()):
        if not isinstance(value, str):
            raise BuildError("Preset environment values must be strings or null")
        for key, replacement in values.items():
            value = value.replace("${" + key + "}", replacement)
        value = re.sub(r"\$penv\{([^}]+)\}", lambda m: parent.get(m[1], ""), value)
        value = re.sub(r"\$env\{([^}]+)\}", lambda m: resolve(m[1], visiting), value)
        if "$env{" in value or "$penv{" in value or "${" in value or "$vendor{" in value:
            raise BuildError("Unsupported preset environment macro")
        return value

    def resolve(name, visiting=()):
        if name in visiting:
            raise BuildError(f"Preset environment cycle: {name}")
        if name not in updates:
            return parent.get(name, "")
        if name not in resolved:
            resolved[name] = "" if updates[name] is None else expand(updates[name], (*visiting, name))
        return resolved[name]

    result = dict(parent)
    for name, value in updates.items():
        if value is None:
            result.pop(name, None)
        else:
            result[name] = resolve(name)
    return result, expand


def git_environment(env=None):
    # Git hooks export repository-local variables. Explicit source selection
    # must not accidentally query the hook's repository/index instead.
    return {key: value for key, value in (os.environ if env is None else env).items()
            if not key.startswith("GIT_")}


def dependency_preflight(root):
    """Catch missing bundled headers, including nested ImPlot, before CMake."""
    if not (root / ".gitmodules").is_file():
        return  # Source archives and standalone CMake test fixtures.
    required = {
        "ext/imgui": ("imgui.h", "imgui.cpp"),
        "ext/imgui_test_engine": ("imgui_test_engine/imgui_te_engine.h",),
        "ext/imgui_test_engine/imgui_test_suite/thirdparty/implot":
            ("implot.h", "implot.cpp", "implot_items.cpp"),
    }
    missing = [str(Path(directory) / name)
               for directory, names in required.items() for name in names
               if not (root / directory / name).is_file()]
    if missing:
        raise BuildError("Dependency preflight failed before configure/build: "
                         + ", ".join(missing)
                         + ". Initialize the reported submodules at their pinned "
                         "revisions (including nested ImPlot); do not use --remote.")
    if (root / ".git").exists():
        result = subprocess.run(
            ["git", "submodule", "status", "--recursive", "--", "ext/imgui",
             "ext/imgui_test_engine"], cwd=root, capture_output=True, text=True,
            check=True, env=git_environment())
        for line in result.stdout.splitlines():
            fields = line[1:].split()
            if len(fields) >= 2 and fields[1] in required and line[0] in "-+U":
                raise BuildError("Dependency submodule is missing or differs from "
                                 "its pinned revision: " + line)


def reuse_checkout(root, selected):
    """Reuse an existing source/cache in place; never move a CMake cache."""
    selected = selected.resolve(strict=True)
    if not selected.is_dir():
        raise BuildError("Reuse source must be a checkout directory")
    heads = []
    for checkout in (root, selected):
        dirty = subprocess.check_output(
            ["git", "status", "--porcelain", "--untracked-files=no"],
            cwd=checkout, text=True, env=git_environment())
        if dirty:
            raise BuildError(f"Reuse source must have clean tracked files: {checkout}")
        heads.append(subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=checkout, text=True,
            env=git_environment()).strip())
    if heads[0] != heads[1]:
        raise BuildError("Reuse source commit differs; use the existing owned "
                         "build lane for an explicitly reviewed source update")
    return selected


def plan(root, args, env, platform):
    data = presets(root)
    build = inherit(data["buildPresets"], args.preset)
    configure_name = build.get("configurePreset", args.preset)
    configure = inherit(data["configurePresets"], configure_name)
    values = {"sourceDir": str(root), "sourceParentDir": str(root.parent),
              "sourceDirName": root.name, "presetName": configure_name,
              "hostSystemName": platform}
    configure_updates = configure.get("environment", {})
    _, expand_directory = preset_environment(env, configure_updates, values)
    build_updates = dict(configure_updates) if build.get("inheritConfigureEnvironment", True) else {}
    build_updates.update(build.get("environment", {}))
    build_env, _ = preset_environment(env, build_updates, {**values, "presetName": args.preset})
    build_env["CMAKE_BUILD_PARALLEL_LEVEL"] = str(args.jobs)
    directory = env.get("YAZE_BUILD_DIR") or configure.get("binaryDir")
    if not isinstance(directory, str) or not directory:
        raise BuildError("Preset has no build directory")
    directory = expand_directory(directory)
    directory = Path(directory)
    if not directory.is_absolute():
        directory = root / directory
    directory = directory.resolve()
    if directory == root:
        raise BuildError("In-source builds are not supported")
    config = build.get("configuration")
    expected_generator = configure.get("generator")
    cache = directory / "CMakeCache.txt"
    configure_command = None
    # Precompiled headers embed absolute paths, so ccache cannot share their
    # objects between worktrees. New agent trees build without them; existing
    # trees keep their recorded value verbatim (CMake, not this script, decides
    # its truth) so a reconfigure never forces a rebuild. The value is always
    # passed explicitly because a preset cache variable would otherwise win.
    disable_pch = "OFF" if env.get("YAZE_AGENT_PCH") == "1" else "ON"
    if cache.is_symlink():
        raise BuildError(f"Build cache must not be a symlink: {cache}")
    if cache.exists():
        cache_values = {}
        for line in cache.read_text().splitlines():
            match = re.match(r"([^:#/][^:]*):[^=]+=(.*)$", line)
            if match:
                cache_values[match[1]] = match[2]
        home = cache_values.get("CMAKE_HOME_DIRECTORY")
        if not home or Path(home).resolve() != root:
            raise BuildError(f"Build cache belongs to another or unknown source: {cache}")
        recorded_dir = cache_values.get("CMAKE_CACHEFILE_DIR")
        if recorded_dir and Path(recorded_dir).resolve() != directory:
            raise BuildError(f"Build cache was moved from another directory: {cache}")
        if expected_generator != cache_values.get("CMAKE_GENERATOR"):
            raise BuildError(f"Build cache generator differs from preset: {cache}")
        configs = cache_values.get("CMAKE_CONFIGURATION_TYPES", "").split(";")
        if config and config not in configs and config != cache_values.get("CMAKE_BUILD_TYPE"):
            raise BuildError(f"Build cache does not contain configuration {config}: {cache}")
        disable_pch = cache_values.get("CMAKE_DISABLE_PRECOMPILE_HEADERS", "OFF")
        if getattr(args, "reuse_from", None):
            for key, value in configure.get("cacheVariables", {}).items():
                # PCH is deliberately preserved verbatim, including --fresh.
                if key == "CMAKE_DISABLE_PRECOMPILE_HEADERS" or value is None:
                    continue
                if isinstance(value, dict):
                    value = value["value"]
                expected = ("ON" if value else "OFF") if isinstance(value, bool) else str(value)
                expected = expand_directory(expected)
                if cache_values.get(key) != expected:
                    raise BuildError(f"Reuse cache option {key} differs from preset: "
                                     f"{cache_values.get(key)!r} != {expected!r}")
    else:
        if directory.exists() and any(directory.iterdir()):
            raise BuildError(f"Nonempty build directory has no CMakeCache.txt: {directory}")
        if not any(getattr(args, key, False)
                   for key in ("allow_new_build", "dry_run", "preflight")):
            raise BuildError(f"No configured cache: {directory}. Reuse an existing "
                             "owned checkout/cache with --reuse-from, or explicitly "
                             "choose --allow-new-build for a justified cold build.")
    print(f"Cache: {'REUSE' if cache.exists() else 'COLD'} {directory}", flush=True)
    print(f"CMAKE_DISABLE_PRECOMPILE_HEADERS: {disable_pch}", flush=True)
    pch_args = [f"-DCMAKE_DISABLE_PRECOMPILE_HEADERS={disable_pch}"]
    if not cache.exists():
        configure_command = ["cmake", "--preset", configure_name, *pch_args, "-B", str(directory)]
    if args.fresh:
        configure_command = ["cmake", "--preset", configure_name, *pch_args, "-B", str(directory), "--fresh"]
    command = ["cmake", "--build", str(directory)]
    if config:
        command += ["--config", config]
    command += ["--target", *args.targets, "--parallel", str(args.jobs)]
    prefix = []
    if platform == "Darwin":
        tools = ("ps",) if args.mode == "desk" else ("taskpolicy", "nice", "ps")
        for tool in tools:
            if (shutil.which(tool, path=env.get("PATH")) is None
                    or shutil.which(tool, path=build_env.get("PATH")) is None):
                raise BuildError(f"Required quiet-build tool is missing: {tool}")
        if args.mode != "desk":
            prefix = ["taskpolicy", *BUILD_MODES[args.mode][2], "nice", "-n", "10"]
    # CMake itself applies the configure preset environment. A --build path
    # (needed for YAZE_BUILD_DIR) requires the inherited build environment here.
    commands = [(prefix + configure_command, env)] if configure_command else []
    return commands + [(prefix + command, build_env)]


def cache_base(root, env):
    if env.get("CCACHE_BASEDIR"):
        base = Path(env["CCACHE_BASEDIR"])
        if not base.is_absolute() or not base.is_dir():
            raise BuildError("CCACHE_BASEDIR must be an existing absolute directory")
        return str(base)
    try:
        result = subprocess.run(["git", "rev-parse", "--git-common-dir"], cwd=root,
                                text=True, capture_output=True, check=True,
                                env=git_environment(env))
        common = Path(result.stdout.strip())
        if not common.is_absolute():
            common = root / common
        repository = common.resolve().parent
        return os.path.commonpath([str(root), str(repository.parent)])
    except (OSError, subprocess.CalledProcessError):
        return str(root.parent)


def build_mode(env):
    path = env.get("OOS_BUILD_MODE_FILE") or Path(env.get("HOME") or Path.home()) / ".config/oos/build_mode"
    try:
        mode = Path(path).read_text().strip()
    except OSError:
        return "laptop"
    return mode if mode in BUILD_MODES else "laptop"


def host_pressure(path):
    """Return (free disk bytes at path, swap used MiB, swap total MiB)."""
    free = shutil.disk_usage(path).free
    try:
        text = subprocess.run(["sysctl", "-n", "vm.swapusage"], text=True,
                              capture_output=True, check=True).stdout
        used, total = (float(re.search(rf"{key} = ([\d.]+)M", text).group(1))
                       for key in ("used", "total"))
    except (OSError, subprocess.SubprocessError, AttributeError):
        used = total = 0.0
    return free, used, total


def mac_pressure_check(root, cold, env):
    if env.get("YAZE_AGENT_BUILD_LOCAL") == "1":
        return
    free, used, total = host_pressure(root)
    reasons = []
    if total and used / total >= MAC_MAX_SWAP_FRACTION:
        reasons.append(f"swap {used / 1024:.1f} of {total / 1024:.1f} GiB used")
    if cold and free < MAC_MIN_FREE_GIB * 2**30:
        reasons.append(f"{free / 2**30:.0f} GiB free disk for a new build tree "
                       f"(needs {MAC_MIN_FREE_GIB})")
    if reasons:
        raise BuildError(
            "Mac build refused: " + "; ".join(reasons) + ". Build and test on the Windows box "
            "instead: scripts/remote/mm-test.sh <branch> [--target T | --filter 'Suite.*'] "
            "(MM_HOST=eva-02 for the second box). For macOS-only proof, rerun with "
            "YAZE_AGENT_BUILD_LOCAL=1.")


def main(argv=None, *, root=ROOT, platform=None):
    platform = platform or host_platform.system()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("targets", nargs="*", default=["yaze"])
    parser.add_argument("--preset", default={"Darwin": "mac-ai", "Linux": "lin-ai"}.get(platform, "win-ai"))
    parser.add_argument("--jobs", type=int,
                        help="workers (desk: all host cores; laptop/unset: default 2, max 4; "
                             "YAZE_BUILD_JOBS overrides the default)")
    parser.add_argument("--dry-run", action="store_true", help="show commands without running them")
    parser.add_argument("--fresh", action="store_true", help="explicitly reconfigure with cmake --fresh")
    parser.add_argument("--preflight", action="store_true",
                        help="validate dependencies/cache and show the plan without building")
    parser.add_argument("--allow-new-build", action="store_true",
                        help="explicitly permit a cold build when no configured cache exists")
    parser.add_argument("--reuse-from", type=Path,
                        help="reuse a clean same-commit checkout's configured build in place")
    parser.add_argument("--source-checkout", type=Path,
                        help="build this explicit owned source with the maintained helper")
    args = parser.parse_args(argv)
    try:
        args.mode = build_mode(os.environ)
        default_jobs, max_jobs, _ = BUILD_MODES[args.mode]
        if args.jobs is None:
            args.jobs = int(os.environ.get("YAZE_BUILD_JOBS") or default_jobs)
        if not 1 <= args.jobs <= max_jobs:
            raise BuildError(f"Agent build jobs must be between 1 and {max_jobs} in {args.mode} mode "
                             f"(default {default_jobs})")
        for target in args.targets:
            if (not re.fullmatch(r"[A-Za-z0-9_.:+-]+", target)
                    or re.match(r"^(all|test|install|package|clean|rebuild_cache|edit_cache)(?:$|[-_:])", target)):
                raise BuildError(f"Select a build target, not a test/install/action target: {target}")
        root = (args.source_checkout or root).resolve(strict=True)
        if not root.is_dir():
            raise BuildError("Source checkout must be a directory")
        if args.reuse_from:
            root = reuse_checkout(root, args.reuse_from)
        dependency_preflight(root)
        env = dict(os.environ)
        # Harnesses commonly inject CMAKE_BUILD_PARALLEL_LEVEL=4 globally.
        # Only an explicit task-specific override changes the quiet default.
        env["CMAKE_BUILD_PARALLEL_LEVEL"] = str(args.jobs)
        env["CCACHE_BASEDIR"] = cache_base(root, env)
        commands = plan(root, args, env, platform)
        print(f"Source: {root}\nPreset: {args.preset}\nBuild mode: {args.mode}\nJobs: {args.jobs}\n"
              f"CCACHE_BASEDIR: {env['CCACHE_BASEDIR']}", flush=True)
        if args.dry_run or args.preflight:
            for command, _ in commands:
                print("[dry-run] " + shlex.join(command))
            return 0
        if platform == "Darwin":
            mac_pressure_check(root, len(commands) > 1, env)
        # Lock before configure and hold through build/owned-child cleanup.
        # The sibling lock does not make a cold cache nonempty. Replan after
        # acquiring it, since another writer may have completed meanwhile.
        build_command = commands[-1][0]
        directory = Path(build_command[build_command.index("--build") + 1])
        with BuildDirectoryLock(directory), Cancellation() as cancellation:
            commands = plan(root, args, env, platform)
            for command, command_env in commands:
                result = cancellation.run(command, root, command_env)
                if result:
                    return result
                if "--preset" in command:
                    args.fresh = False
                    if len(plan(root, args, env, platform)) != 1:
                        raise BuildError("Configure did not create a valid build cache")
            return 0
    except (BuildError, OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as error:
        print(f"agent_build: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
