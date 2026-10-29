#!/usr/bin/env python3
"""Bind and verify real Project Save As on an already copied intact bundle.

The owned harness supplies only the destination-picker result via
YAZE_TEST_PROJECT_SAVE_AS. No descriptors, sidecars or ROMs are written here.
"""
import argparse
import configparser
import json
from pathlib import Path

from prepare_authoring_bundle_fixture import inventory, outside, sha256, write_new


def references(project):
    parser = configparser.ConfigParser(interpolation=None, strict=False)
    parser.read(project)
    return {key: (project.parent / value).resolve()
            for key, value in parser.items("files") if value}


def prepare(bundle, destination, output):
    bundle = bundle.resolve()
    destination = destination.absolute()
    if (destination.resolve() != destination or
            destination.suffix != ".yaze" or destination.exists() or
            bundle == destination.parent or bundle in destination.parents):
        raise ValueError("Use a fresh canonical cross-directory .yaze destination")
    output = outside(output, bundle)
    if destination.with_suffix(".lyra-terrain.json").exists():
        raise ValueError("Destination sidecar must not exist")
    snapshot = inventory(bundle)
    refs = references(bundle / "project.yaze")
    for key in ("lyra_terrain_file", "lyra_build_request_file", "rom_filename"):
        if key not in refs or not refs[key].is_file():
            raise ValueError("Missing populated resource: " + key)
    output.mkdir(parents=True, exist_ok=False)
    binding = {"bundle": str(bundle), "destination": str(destination),
               "inventory": snapshot,
               "references": {key: str(path) for key, path in refs.items()},
               "sidecar_sha256": sha256(refs["lyra_terrain_file"])}
    write_new(output / "binding.json", binding)
    write_new(output / "save-as.json", {
        "schema_version": 1, "name": "Current bundle normal Project Save As",
        "steps": [{"action": "click", "target": "shortcut:Save As",
                   "expect": {"success": True, "status": "passed"}}]})
    return binding


def verify(output, cancelled=False):
    binding = json.loads((output / "binding.json").read_text())
    bundle = Path(binding["bundle"])
    if inventory(bundle) != binding["inventory"]:
        raise ValueError("Original bundle/resource/BIN identity changed")
    destination = Path(binding["destination"])
    if cancelled:
        if destination.exists() or destination.with_suffix(".lyra-terrain.json").exists():
            raise ValueError("Cancel created a destination descriptor")
        return {"cancelled": True, "source_unchanged": True}
    if not destination.is_file():
        raise ValueError("Project Save As did not create its destination")
    refs = references(destination)
    for key, expected in binding["references"].items():
        if key not in refs:
            raise ValueError("Relocated project lost resource: " + key)
        if key == "lyra_terrain_file":
            if refs[key] == Path(expected) or sha256(refs[key]) != binding["sidecar_sha256"]:
                raise ValueError("Relocated sidecar identity mismatch")
        elif refs[key] != Path(expected):
            raise ValueError("Relocated resource/request identity mismatch: " + key)
    return {"destination": str(destination), "sha256": sha256(destination),
            "references": len(refs), "source_unchanged": True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("prepare", "verify", "verify-cancel"))
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--bundle", type=Path)
    parser.add_argument("--destination", type=Path)
    args = parser.parse_args()
    if args.mode == "prepare":
        if not args.bundle or not args.destination:
            parser.error("prepare requires --bundle and --destination")
        result = prepare(args.bundle, args.destination, args.output)
    else:
        result = verify(args.output, args.mode == "verify-cancel")
    print(json.dumps(result, sort_keys=True))


if __name__ == "__main__":
    main()
