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
# Shared with ~/.local/bin/quiet-build: (default jobs, maximum jobs, QoS flags).
# Desk builds may use P-cores; laptop builds stay on background QoS.
BUILD_MODES = {"desk": (8, 8, ["-c", "utility"]), "laptop": (2, 4, ["-b"])}


class BuildError(Exception):
    pass


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
    else:
        if directory.exists() and any(directory.iterdir()):
            raise BuildError(f"Nonempty build directory has no CMakeCache.txt: {directory}")
        configure_command = ["cmake", "--preset", configure_name, "-B", str(directory)]
    if args.fresh:
        configure_command = ["cmake", "--preset", configure_name, "-B", str(directory), "--fresh"]
    command = ["cmake", "--build", str(directory)]
    if config:
        command += ["--config", config]
    command += ["--target", *args.targets, "--parallel", str(args.jobs)]
    prefix = []
    if platform == "Darwin":
        for tool in ("taskpolicy", "nice", "ps"):
            if (shutil.which(tool, path=env.get("PATH")) is None
                    or shutil.which(tool, path=build_env.get("PATH")) is None):
                raise BuildError(f"Required quiet-build tool is missing: {tool}")
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
                                text=True, capture_output=True, check=True)
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


def main(argv=None, *, root=ROOT, platform=None):
    platform = platform or host_platform.system()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("targets", nargs="*", default=["yaze"])
    parser.add_argument("--preset", default={"Darwin": "mac-ai", "Linux": "lin-ai"}.get(platform, "win-ai"))
    parser.add_argument("--jobs", type=int,
                        help="workers (desk mode: default/max 8; laptop or unset: default 2, max 4; "
                             "YAZE_BUILD_JOBS overrides the default)")
    parser.add_argument("--dry-run", action="store_true", help="show commands without running them")
    parser.add_argument("--fresh", action="store_true", help="explicitly reconfigure with cmake --fresh")
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
        root = root.resolve()
        env = dict(os.environ)
        # Harnesses commonly inject CMAKE_BUILD_PARALLEL_LEVEL=4 globally.
        # Only an explicit task-specific override changes the quiet default.
        env["CMAKE_BUILD_PARALLEL_LEVEL"] = str(args.jobs)
        env["CCACHE_BASEDIR"] = cache_base(root, env)
        commands = plan(root, args, env, platform)
        print(f"Source: {root}\nPreset: {args.preset}\nBuild mode: {args.mode}\nJobs: {args.jobs}\n"
              f"CCACHE_BASEDIR: {env['CCACHE_BASEDIR']}", flush=True)
        if args.dry_run:
            for command, _ in commands:
                print("[dry-run] " + shlex.join(command))
            return 0
        with Cancellation() as cancellation:
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
