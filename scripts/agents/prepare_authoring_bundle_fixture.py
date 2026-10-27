#!/usr/bin/env python3
"""Prepare an intact, pinned authoring bundle for a separately admitted GUI test.

This does not launch an app, alter descriptors, edit a ROM, or qualify editing.
Capture an inventory, review its hash, then pass that exact hash to prepare.
"""

import argparse
import configparser
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import stat


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(65536), b""):
            digest.update(chunk)
    return digest.hexdigest()


def root_path(path):
    path = Path(path).absolute()
    if path.resolve() != path or not path.is_dir():
        raise ValueError("bundle must be a canonical directory without symlinks")
    return path


def outside(path, root):
    path = Path(path).absolute()
    if path.resolve() != path or path == root or root in path.parents:
        raise ValueError("output must be canonical and outside the source bundle")
    return path


def inventory(bundle):
    bundle = root_path(bundle)
    files, directories = {}, []
    for current, dirs, names in os.walk(bundle, followlinks=False):
        for name in sorted(dirs + names):
            path = Path(current) / name
            info = path.lstat()
            relative = path.relative_to(bundle).as_posix()
            if stat.S_ISDIR(info.st_mode):
                directories.append(relative)
            elif stat.S_ISREG(info.st_mode):
                files[relative] = {"sha256": sha256(path), "bytes": info.st_size,
                                   "mode": stat.S_IMODE(info.st_mode)}
            else:
                raise ValueError("unsupported symlink or special file: " + relative)
    for required in ("project.yaze", "rom", "manifest.json"):
        if required not in files:
            raise ValueError("missing bundle file: " + required)
    parser = configparser.ConfigParser(interpolation=None, strict=False)
    parser.read(bundle / "project.yaze")
    for key, value in parser.items("files"):
        if not value:
            continue
        relative = PurePosixPath(value)
        if relative.is_absolute() or ".." in relative.parts or "\\" in value:
            raise ValueError("nonportable project path: files." + key)
        if not (bundle / value).exists():
            raise ValueError("missing project resource: files." + key)
    return {"schema_version": 1, "bundle": str(bundle),
            "files": dict(sorted(files.items())),
            "directories": sorted(directories)}


def write_new(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("x", encoding="utf-8") as stream:
        json.dump(value, stream, indent=2, sort_keys=True)
        stream.write("\n")


def prepare(bundle, inventory_path, expected_hash, destination, receipt):
    bundle = root_path(bundle)
    destination = outside(destination, bundle)
    receipt = outside(receipt, bundle)
    if destination in bundle.parents or destination == receipt or destination in receipt.parents:
        raise ValueError("copy and receipt must be separate from source and each other")
    if destination.exists() or receipt.exists():
        raise ValueError("destination and receipt must not exist")
    raw = Path(inventory_path).read_bytes()
    if len(expected_hash) != 64 or hashlib.sha256(raw).hexdigest() != expected_hash:
        raise ValueError("reviewed inventory SHA-256 mismatch")
    expected = json.loads(raw)
    if inventory(bundle) != expected:
        raise ValueError("source bundle changed since inventory")
    shutil.copytree(bundle, destination, copy_function=shutil.copy2)
    copied = inventory(destination)
    copied["bundle"] = str(bundle)
    if copied != expected or inventory(bundle) != expected:
        raise ValueError("copy/source readback mismatch; retain copy for diagnosis")
    for relative in expected["files"]:
        if os.path.samefile(bundle / relative, destination / relative):
            raise ValueError("copy aliases original file: " + relative)
    result = {"schema_version": 1, "status": "FIXTURE_PREPARED",
              "acceptance": "UNRUN", "native_launched": False,
              "source_bundle": str(bundle), "destination": str(destination),
              "inventory_sha256": expected_hash,
              "files": len(expected["files"]),
              "bytes": sum(row["bytes"] for row in expected["files"].values()),
              "next": "Review/admit normal GUI replay on this intact copy."}
    write_new(receipt, result)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    capture = commands.add_parser("inventory")
    capture.add_argument("--bundle", required=True)
    capture.add_argument("--output", required=True)
    copy = commands.add_parser("prepare")
    for option in ("bundle", "inventory", "inventory-sha256", "destination", "receipt"):
        copy.add_argument("--" + option, required=True)
    args = parser.parse_args()
    try:
        if args.command == "inventory":
            bundle = root_path(args.bundle)
            output = outside(args.output, bundle)
            write_new(output, inventory(bundle))
            print(json.dumps({"inventory": str(output), "sha256": sha256(output)}))
        else:
            print(json.dumps(prepare(args.bundle, args.inventory,
                                     args.inventory_sha256, args.destination,
                                     args.receipt)))
    except (ValueError, OSError, configparser.Error) as error:
        parser.exit(1, "error: " + str(error) + "\n")


if __name__ == "__main__":
    main()
