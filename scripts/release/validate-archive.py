#!/usr/bin/env python3
import argparse
import json
import os
import pathlib
import subprocess
import tarfile
import tempfile
import zipfile


BANNED_HELPERS = {
    "overworld_golden_data_extractor",
    "extract_vanilla_values",
    "rom_patch_utility",
    "dungeon_test_harness",
}
BANNED_WIN_DLL_PREFIXES = ("api-ms-win-", "ext-ms-win-")
BANNED_WIN_DLLS = {"ucrtbase.dll"}

REQUIRED_FILES = ("README.md", "LICENSE", "manifest.json")
REQUIRED_ASSETS_PREFIX = "assets/"
REQUIRED_YAZE = ("yaze", "yaze.exe")
REQUIRED_Z3ED = ("z3ed", "z3ed.exe")
LINUX_YAZE = "usr/bin/yaze"
LINUX_Z3ED = "usr/bin/z3ed"
LINUX_MANIFEST = "usr/share/yaze/manifest.json"
LINUX_ASSETS_PREFIX = "usr/share/yaze/assets/"
LINUX_README = "usr/share/doc/yaze/README.md"
LINUX_LICENSE = "usr/share/doc/yaze/LICENSE"


def normalize_path(path: str) -> str:
    stripped = path.lstrip("./")
    return stripped.replace("\\", "/")


def detect_root(entries):
    parts = [p.split("/", 1)[0] for p in entries if p]
    if not parts:
        return ""
    root = parts[0]
    if all(part == root for part in parts):
        return root
    return ""


def join_root(root, rel):
    return f"{root}/{rel}" if root else rel


def has_file(entries, root, rel):
    return join_root(root, rel) in entries


def has_prefix(entries, root, rel_prefix):
    prefix = join_root(root, rel_prefix)
    return any(entry.startswith(prefix) for entry in entries)


def has_any(entries, root, options):
    return any(has_file(entries, root, option) for option in options)


def list_zip_entries(path):
    """Return entries list, file entries, and an open ZipFile handle.

    Caller is responsible for closing the handle.
    """
    zf = zipfile.ZipFile(path)
    entries = [normalize_path(name) for name in zf.namelist()]
    file_entries = [name for name in entries if name and not name.endswith("/")]
    return entries, file_entries, zf


def list_tar_entries(path):
    """Return entries list, file entries, and an open TarFile handle.

    Caller is responsible for closing the handle.
    """
    tf = tarfile.open(path)
    members = []
    files = []
    for member in tf.getmembers():
        if not member.name:
            continue
        name = normalize_path(member.name)
        members.append(name)
        if member.isfile():
            files.append(name)
    return members, files, tf


def list_deb_entries(path):
    try:
        output = subprocess.check_output(
            ["dpkg-deb", "-c", path], text=True, stderr=subprocess.STDOUT
        )
    except (FileNotFoundError, subprocess.CalledProcessError):
        return None
    entries = []
    for line in output.splitlines():
        parts = line.split()
        if len(parts) >= 6:
            entries.append(normalize_path(parts[-1]))
    return entries


def load_manifest_from_zip(zf, manifest_path):
    with zf.open(manifest_path) as handle:
        return json.load(handle)


def load_manifest_from_tar(tf, manifest_path):
    member = tf.getmember(manifest_path)
    with tf.extractfile(member) as handle:
        return json.load(handle)


def validate_manifest(manifest):
    required_keys = ("name", "version", "git_sha", "features")
    for key in required_keys:
        if key not in manifest:
            raise ValueError(f"manifest.json missing '{key}'")
    if not isinstance(manifest.get("features"), dict):
        raise ValueError("manifest.json 'features' must be an object")


def validate_expected_manifest(manifest, expected_version, expected_git_sha):
    errors = []

    if expected_version:
        expected = expected_version.removeprefix("v")
        actual = str(manifest.get("version", "")).removeprefix("v")
        if actual != expected:
            errors.append(
                f"manifest.json version mismatch: expected {expected}, found {actual}"
            )

    if expected_git_sha:
        expected = expected_git_sha.lower()
        actual = str(manifest.get("git_sha", "")).lower()
        if len(actual) < 7 or not expected.startswith(actual):
            errors.append(
                "manifest.json git_sha mismatch: "
                f"expected prefix of {expected}, found {actual}"
            )

    return errors


def validate_common(entries, files, root):
    errors = []
    for rel in REQUIRED_FILES:
        if not has_file(files, root, rel):
            errors.append(f"missing required file: {rel}")
    if not has_prefix(files, root, REQUIRED_ASSETS_PREFIX):
        errors.append("missing asset files")
    if not has_any(files, root, REQUIRED_YAZE):
        errors.append("missing yaze binary")
    if not has_any(files, root, REQUIRED_Z3ED):
        errors.append("missing z3ed binary")

    banned_bin = join_root(root, "bin/")
    if any(entry.startswith(banned_bin) for entry in entries):
        errors.append("contains bin/ directory")

    for entry in files:
        base = os.path.basename(entry)
        if base in BANNED_HELPERS or base in {f"{name}.exe" for name in BANNED_HELPERS}:
            errors.append(f"contains helper tool: {base}")

    return errors


def validate_windows_dlls(entries):
    errors = []
    dll_names = {os.path.basename(entry).lower() for entry in entries}
    for entry in entries:
        base = os.path.basename(entry).lower()
        if any(base.startswith(prefix) for prefix in BANNED_WIN_DLL_PREFIXES):
            errors.append(f"contains unexpected DLL: {entry}")
        if base in BANNED_WIN_DLLS:
            errors.append(f"contains unexpected DLL: {entry}")
    for required_prefix in ("msvcp", "vcruntime"):
        if not any(
            name.startswith(required_prefix) and name.endswith(".dll")
            for name in dll_names
        ):
            errors.append(f"missing app-local {required_prefix} runtime DLL")
    return errors


def validate_linux_layout(entries, files, root):
    errors = []
    required_files = (
        LINUX_YAZE,
        LINUX_Z3ED,
        LINUX_MANIFEST,
        LINUX_README,
        LINUX_LICENSE,
    )
    for rel in required_files:
        if not has_file(files, root, rel):
            errors.append(f"missing required Linux package file: {rel}")

    if not has_prefix(files, root, LINUX_ASSETS_PREFIX):
        errors.append(f"missing Linux package asset files under {LINUX_ASSETS_PREFIX}")

    expected_binaries = {"yaze": LINUX_YAZE, "z3ed": LINUX_Z3ED}
    for binary_name, expected_path in expected_binaries.items():
        matches = [entry for entry in files if os.path.basename(entry) == binary_name]
        if len(matches) != 1:
            errors.append(
                f"expected exactly one {binary_name} binary, found {len(matches)}"
            )
        elif matches[0] != join_root(root, expected_path):
            errors.append(
                f"{binary_name} binary is at {matches[0]}, expected "
                f"{join_root(root, expected_path)}"
            )

    for entry in files:
        base = os.path.basename(entry)
        if base in BANNED_HELPERS or base in {f"{name}.exe" for name in BANNED_HELPERS}:
            errors.append(f"contains helper tool: {base}")

    return errors


def load_manifest_from_deb(path):
    with tempfile.TemporaryDirectory(prefix="yaze-deb-manifest-") as extract_dir:
        subprocess.run(
            ["dpkg-deb", "-x", path, extract_dir],
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
            text=True,
        )
        manifest_path = pathlib.Path(extract_dir) / LINUX_MANIFEST
        if not manifest_path.is_file():
            raise ValueError(f"missing {LINUX_MANIFEST}")
        with manifest_path.open(encoding="utf-8") as handle:
            return json.load(handle)


def read_deb_field(path, field):
    return subprocess.check_output(
        ["dpkg-deb", "--field", path, field],
        text=True,
        stderr=subprocess.STDOUT,
    ).strip()


def validate_deb_metadata(path, expected_version):
    errors = []
    try:
        package = read_deb_field(path, "Package")
        version = read_deb_field(path, "Version")
        architecture = read_deb_field(path, "Architecture")
        depends = read_deb_field(path, "Depends")
        description = read_deb_field(path, "Description")
    except (FileNotFoundError, subprocess.CalledProcessError) as exc:
        return [f"unable to read Debian control metadata: {exc}"]

    if package != "yaze":
        errors.append(f"Debian Package must be 'yaze', found '{package}'")
    if expected_version:
        expected = expected_version.removeprefix("v")
        actual = version.removeprefix("v")
        if actual != expected:
            errors.append(
                f"Debian Version mismatch: expected {expected}, found {actual}"
            )
    if not architecture or architecture == "all":
        errors.append(
            f"Debian Architecture must identify a native target, found '{architecture}'"
        )
    if not depends:
        errors.append("Debian Depends field is empty")
    if not description:
        errors.append("Debian Description field is empty")

    return errors


def validate_archive(path, expected_version=None, expected_git_sha=None):
    errors = []
    lower = path.lower()
    if lower.endswith(".zip"):
        entries, files, zf = list_zip_entries(path)
        try:
            root = detect_root(entries)
            errors.extend(validate_common(entries, files, root))
            manifest_path = join_root(root, "manifest.json")
            if manifest_path in files:
                try:
                    manifest = load_manifest_from_zip(zf, manifest_path)
                    validate_manifest(manifest)
                    errors.extend(
                        validate_expected_manifest(
                            manifest, expected_version, expected_git_sha
                        )
                    )
                except Exception as exc:  # noqa: BLE001
                    errors.append(f"manifest.json invalid: {exc}")
            errors.extend(validate_windows_dlls(files))
        finally:
            zf.close()
        return errors

    if lower.endswith(".tar.gz") or lower.endswith(".tgz"):
        entries, files, tf = list_tar_entries(path)
        try:
            root = "" if LINUX_YAZE in files else detect_root(entries)
            errors.extend(validate_linux_layout(entries, files, root))
            manifest_path = join_root(root, LINUX_MANIFEST)
            if manifest_path in files:
                try:
                    manifest = load_manifest_from_tar(tf, manifest_path)
                    validate_manifest(manifest)
                    errors.extend(
                        validate_expected_manifest(
                            manifest, expected_version, expected_git_sha
                        )
                    )
                except Exception as exc:  # noqa: BLE001
                    errors.append(f"manifest.json invalid: {exc}")
        finally:
            tf.close()
        return errors

    if lower.endswith(".deb"):
        entries = list_deb_entries(path)
        if entries is None:
            return ["dpkg-deb unavailable for .deb validation"]
        files = [entry for entry in entries if not entry.endswith("/")]
        errors.extend(validate_linux_layout(entries, files, ""))
        errors.extend(validate_deb_metadata(path, expected_version))
        try:
            manifest = load_manifest_from_deb(path)
            validate_manifest(manifest)
            errors.extend(
                validate_expected_manifest(manifest, expected_version, expected_git_sha)
            )
        except Exception as exc:  # noqa: BLE001
            errors.append(f"manifest.json invalid: {exc}")
        return errors

    return [f"unsupported archive format: {path}"]


def main():
    parser = argparse.ArgumentParser(description="Validate release archives.")
    parser.add_argument(
        "--expected-version",
        help="Required manifest version (an optional leading v is ignored).",
    )
    parser.add_argument(
        "--expected-git-sha",
        help="Required full Git SHA; the manifest may contain its short prefix.",
    )
    parser.add_argument("archives", nargs="+", help="Archive paths to validate.")
    args = parser.parse_args()

    failures = []
    for archive in args.archives:
        if not os.path.exists(archive):
            failures.append(f"{archive}: not found")
            continue
        errors = validate_archive(
            archive,
            expected_version=args.expected_version,
            expected_git_sha=args.expected_git_sha,
        )
        if errors:
            failures.append(f"{archive}: " + "; ".join(errors))

    if failures:
        print("Release artifact validation failed:")
        for failure in failures:
            print(f" - {failure}")
        return 1

    print("Release artifact validation passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
