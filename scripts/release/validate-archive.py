#!/usr/bin/env python3
import argparse
import json
import os
import subprocess
import sys
import tarfile
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
    members = [normalize_path(m.name) for m in tf.getmembers() if m.name]
    files = [name for name in members if not name.endswith("/")]
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


def validate_common(entries, files, root):
    errors = []
    for rel in REQUIRED_FILES:
        if not has_file(files, root, rel):
            errors.append(f"missing required file: {rel}")
    if not has_prefix(entries, root, REQUIRED_ASSETS_PREFIX):
        errors.append("missing assets directory")
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
    for entry in entries:
        base = os.path.basename(entry).lower()
        if any(base.startswith(prefix) for prefix in BANNED_WIN_DLL_PREFIXES):
            errors.append(f"contains unexpected DLL: {entry}")
        if base in BANNED_WIN_DLLS:
            errors.append(f"contains unexpected DLL: {entry}")
    return errors


def validate_archive(path):
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
                except Exception as exc:  # noqa: BLE001
                    errors.append(f"manifest.json invalid: {exc}")
            errors.extend(validate_windows_dlls(files))
        finally:
            zf.close()
        return errors

    if lower.endswith(".tar.gz") or lower.endswith(".tgz"):
        entries, files, tf = list_tar_entries(path)
        try:
            root = detect_root(entries)
            errors.extend(validate_common(entries, files, root))
            manifest_path = join_root(root, "manifest.json")
            if manifest_path in files:
                try:
                    manifest = load_manifest_from_tar(tf, manifest_path)
                    validate_manifest(manifest)
                except Exception as exc:  # noqa: BLE001
                    errors.append(f"manifest.json invalid: {exc}")
        finally:
            tf.close()
        return errors

    if lower.endswith(".deb"):
        entries = list_deb_entries(path)
        if entries is None:
            return ["dpkg-deb unavailable for .deb validation"]
        root = detect_root(entries)
        errors.extend(validate_common(entries, entries, root))
        return errors

    return [f"unsupported archive format: {path}"]


def main():
    parser = argparse.ArgumentParser(description="Validate release archives.")
    parser.add_argument("archives", nargs="+", help="Archive paths to validate.")
    args = parser.parse_args()

    failures = []
    for archive in args.archives:
        if not os.path.exists(archive):
            failures.append(f"{archive}: not found")
            continue
        errors = validate_archive(archive)
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
