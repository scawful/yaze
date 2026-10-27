#!/usr/bin/env python3
"""Build selected targets quietly, reusing a preset and the macOS heavy lane."""

import argparse
import json
import math
import os
from pathlib import Path
import platform as host_platform
import re
import shlex
import shutil
import signal
import stat
import subprocess
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[2]
LOCK_PATH = Path("/tmp/oos-heavy.lock")


class BuildError(Exception):
    pass


def read_regular(directory_fd, name, limit=16384):
    fd = os.open(name, os.O_RDONLY | os.O_NOFOLLOW, dir_fd=directory_fd)
    with os.fdopen(fd, "rb") as stream:
        info = os.fstat(stream.fileno())
        if not stat.S_ISREG(info.st_mode) or info.st_size > limit:
            raise BuildError(f"Invalid heavy-lock file: {name}")
        return stream.read(limit + 1).decode("utf-8")


def lock_status(path):
    try:
        fd = os.open(path, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    except FileNotFoundError:
        return None
    try:
        info = os.fstat(fd)
        has_metadata = True
        try:
            metadata = json.loads(read_regular(fd, "metadata.json"))
        except FileNotFoundError:
            has_metadata = False
            metadata = None  # Existing clients write only the plain owner file.
        except (ValueError, UnicodeError) as error:
            raise BuildError(f"Malformed heavy-lock metadata: {path}") from error
        # Metadata is published after owner. Read it first to avoid pairing a
        # new complete metadata file with a pre-publication missing owner.
        try:
            owner = read_regular(fd, "owner", 4096).strip() or "?"
        except FileNotFoundError:
            owner = "?"
        if isinstance(metadata, dict) and metadata.get("owner") != owner:
            try:
                # An owner may release metadata then owner between our reads.
                read_regular(fd, "metadata.json")
            except FileNotFoundError:
                has_metadata, metadata = False, None
        if has_metadata:
            if (not isinstance(metadata, dict)
                    or type(metadata.get("schema")) is not int
                    or metadata["schema"] != 1
                    or not isinstance(metadata.get("token"), str)
                    or not re.fullmatch(r"[a-f0-9]{32}", metadata["token"])
                    or type(metadata.get("pid")) is not int
                    or metadata["pid"] <= 0
                    or metadata.get("owner") != owner
                    or type(metadata.get("started_at")) not in (int, float)
                    or not math.isfinite(metadata["started_at"])
                    or metadata["started_at"] <= 0):
                raise BuildError(f"Malformed heavy-lock metadata: {path}")
        if os.fstat(fd).st_nlink == 0:
            return None  # The directory was released during inspection.
        return {"owner": owner, "metadata": metadata,
                "age_seconds": max(0, int(time.time() - info.st_mtime)),
                "identity": (info.st_dev, info.st_ino)}
    finally:
        os.close(fd)


def describe_lock(state):
    if state is None:
        return "Heavy lane: free"
    metadata = state["metadata"]
    pid = f", pid={metadata['pid']}" if metadata else ", legacy/unknown metadata"
    return f"Heavy lane: owner={state['owner']}{pid}, age={state['age_seconds']}s"


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
                    # Escalate only our newly created process group. The lane
                    # stays held until its leader AND descendants have exited.
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


class HeavyLane:
    def __init__(self, path, owner, root):
        self.path = path
        self.owner = owner
        self.metadata = {"schema": 1, "token": uuid.uuid4().hex,
                         "pid": os.getpid(), "owner": owner,
                         "started_at": time.time(), "source": str(root)}
        self.identity = None

    def acquire(self, wait_seconds, cancellation):
        started = time.monotonic()
        last_owner = None
        last_report = started
        while True:
            cancellation.check()
            try:
                self.path.mkdir()
            except FileExistsError:
                state = lock_status(self.path)
                if state is None:
                    continue  # Owner released between mkdir and inspection.
                signature = (state["identity"], state["owner"])
                now = time.monotonic()
                if signature != last_owner or now - last_report >= 30:
                    print(f"{describe_lock(state)}; waited {now - started:.1f}s", flush=True)
                    last_owner, last_report = signature, now
                if now - started >= wait_seconds:
                    raise BuildError(f"Heavy lane remained busy after {now - started:.1f}s; no lock was changed")
                time.sleep(min(2.0, wait_seconds - (now - started)))
                continue
            info = self.path.stat()
            self.identity = (info.st_dev, info.st_ino)
            # Signal handlers only record cancellation, so publication finishes.
            # Unknown/partial locks are never reclaimed by another invocation.
            with (self.path / "owner").open("x") as stream:
                stream.write(self.owner + "\n")
            pending = self.path / (".metadata-" + self.metadata["token"] + ".tmp")
            with pending.open("x") as stream:
                json.dump(self.metadata, stream)
                stream.write("\n")
            pending.replace(self.path / "metadata.json")
            print(f"Acquired heavy lane after {time.monotonic() - started:.1f}s", flush=True)
            return

    def release(self):
        if self.identity is None:
            return
        state = lock_status(self.path)
        if (state is None or state["identity"] != self.identity
                or state["metadata"] != self.metadata
                or state["owner"] != self.owner):
            raise BuildError("Heavy lock identity changed; refusing to release it")
        fd = os.open(self.path, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
        try:
            info = os.fstat(fd)
            if (info.st_dev, info.st_ino) != self.identity:
                raise BuildError("Heavy lock identity changed during release")
            if set(os.listdir(fd)) != {"owner", "metadata.json"}:
                raise BuildError("Unknown heavy-lock contents; refusing to release it")
            os.unlink("metadata.json", dir_fd=fd)
            os.unlink("owner", dir_fd=fd)
            os.rmdir(self.path)
            self.identity = None
        finally:
            os.close(fd)


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
    # Precompiled headers embed absolute paths, so ccache cannot share their
    # objects between worktrees. New agent trees build without them; existing
    # trees keep their recorded choice so a reconfigure never forces a rebuild.
    disable_pch = env.get("YAZE_AGENT_PCH") != "1"
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
        disable_pch = cache_values.get("CMAKE_DISABLE_PRECOMPILE_HEADERS", "").upper() in ("ON", "1", "TRUE")
    else:
        if directory.exists() and any(directory.iterdir()):
            raise BuildError(f"Nonempty build directory has no CMakeCache.txt: {directory}")
    pch_args = ["-DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON"] if disable_pch else []
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
        for tool in ("taskpolicy", "nice", "ps"):
            if (shutil.which(tool, path=env.get("PATH")) is None
                    or shutil.which(tool, path=build_env.get("PATH")) is None):
                raise BuildError(f"Required quiet-build tool is missing: {tool}")
        prefix = ["taskpolicy", "-b", "nice", "-n", "10"]
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


def main(argv=None, *, root=ROOT, lock_path=LOCK_PATH, platform=None):
    platform = platform or host_platform.system()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("targets", nargs="*", default=["yaze"])
    parser.add_argument("--preset", default={"Darwin": "mac-ai", "Linux": "lin-ai"}.get(platform, "win-ai"))
    parser.add_argument("--jobs", type=int, default=os.environ.get("YAZE_BUILD_JOBS", "2"),
                        help="workers, 1–4 (default 2; YAZE_BUILD_JOBS overrides)")
    parser.add_argument("--wait-seconds", type=int, default=600,
                        help="wait for the heavy lane (0–3600; default 600)")
    parser.add_argument("--status", action="store_true", help="read heavy-lane owner and age, then exit")
    parser.add_argument("--dry-run", action="store_true", help="show commands without taking the lane")
    parser.add_argument("--fresh", action="store_true", help="explicitly reconfigure with cmake --fresh")
    args = parser.parse_args(argv)
    try:
        if args.status:
            print(describe_lock(lock_status(lock_path)))
            return 0
        if not 1 <= args.jobs <= 4:
            raise BuildError("Agent build jobs must be between 1 and 4 (default 2)")
        if not 0 <= args.wait_seconds <= 3600:
            raise BuildError("Wait must be between 0 and 3600 seconds")
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
        print(f"Source: {root}\nPreset: {args.preset}\nJobs: {args.jobs}\n"
              f"CCACHE_BASEDIR: {env['CCACHE_BASEDIR']}", flush=True)
        if args.dry_run:
            if platform == "Darwin":
                print(describe_lock(lock_status(lock_path)))
            for command, _ in commands:
                print("[dry-run] " + shlex.join(command))
            return 0
        owner = env.get("YAZE_BUILD_OWNER", f"yaze-agent-build-{os.getpid()}")
        if not owner.strip() or owner != owner.strip() or len(owner) > 512 or "\n" in owner or "\r" in owner:
            raise BuildError("YAZE_BUILD_OWNER must be one nonempty line of at most 512 characters")
        lane = HeavyLane(lock_path, owner, root) if platform == "Darwin" else None
        with Cancellation() as cancellation:
            try:
                if lane:
                    lane.acquire(args.wait_seconds, cancellation)
                cancellation.check()
                # Revalidate source/config after waiting for another owner.
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
            finally:
                if lane and cancellation.child is None:
                    lane.release()
                elif lane:
                    print("Build process group exit is unconfirmed; retaining own lock", file=sys.stderr)
    except (BuildError, OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as error:
        print(f"agent_build: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
