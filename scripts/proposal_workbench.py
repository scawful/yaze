#!/usr/bin/env python3
"""Prepare editable proposal bundles; Yaze remains the only model/save writer.

The packet is local and contains private ROM/source data. It is not a share ZIP.
No operation writes the original project, imports pixel mockups, or launches apps.
"""
import argparse
import configparser
import hashlib
import json
from pathlib import Path
import shutil
import sys

SCHEMA = "yaze.proposal-workbench.v1"


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def write_json(path, value):
    with path.open("x", encoding="utf-8") as stream:
        json.dump(value, stream, indent=2, sort_keys=True)
        stream.write("\n")


def inventory(root):
    result = {}
    for path in sorted(root.rglob("*")):
        if path.is_symlink():
            raise ValueError(f"Symlink is not an isolated proposal asset: {path}")
        if path.is_file():
            result[path.relative_to(root).as_posix()] = digest(path)
        elif not path.is_dir():
            raise ValueError(f"Unsupported bundle entry: {path}")
    return result


def within(root, value):
    path = (root / value).resolve()
    if not path.is_relative_to(root):
        raise ValueError(f"Project path escapes the bundle: {value}")
    return path


def descriptor(path):
    config = configparser.ConfigParser(interpolation=None, strict=True)
    config.read_string(path.read_text(encoding="utf-8"))
    return config


def validate_paths(root, config):
    # Existing Yaze bundle-relative contract. Lists need their own resolver;
    # refuse them instead of accidentally retaining a shared edit target.
    for key, value in config.items("files"):
        if not value:
            continue
        if key == "additional_roms":
            raise ValueError("Multiple-ROM proposals need explicit per-ROM custody")
        within(root, value)
    for key in ("output_folder", "build_target", "asm_entry_point", "git_repository"):
        value = config.get("build", key, fallback="")
        if value:
            within(root, value)
    if config.get("build", "asm_sources", fallback=""):
        raise ValueError("Explicit asm_sources lists are not supported in v1")
    for key in ("build_script", "run_script"):
        command = config.get("build", key, fallback="")
        # Commands are trusted project content, never run by this helper.
        # Require relocation-aware variables for an enabled workflow.
        if command and "$YAZE_PROJECT_" not in command:
            raise ValueError(f"{key} must use YAZE_PROJECT_* variables")
    rom = within(root, config.get("files", "rom_filename"))
    if not rom.is_file():
        raise ValueError("The editing ROM is missing")
    spec = config.get("graphics_sheets", "source_spec", fallback="")
    if spec:
        code = within(root, config.get("files", "code_folder"))
        within(code, spec)
    built = config.get("build", "build_target", fallback="")
    if built and within(root, built) == rom:
        raise ValueError("Build target must differ from the authoring ROM")
    return rom


def replace_fields(text, changes):
    """Keep all unrelated labels, comments, ordering and settings verbatim."""
    section = ""
    out = []
    found = set()
    for line in text.splitlines(keepends=True):
        stripped = line.strip()
        if stripped.startswith("[") and stripped.endswith("]"):
            section = stripped[1:-1]
        if "=" in line and not stripped.startswith(("#", ";")):
            key = line.split("=", 1)[0].strip()
            pair = (section, key)
            if pair in changes:
                line = f"{key}={changes[pair]}\n"
                found.add(pair)
        out.append(line)
    return "".join(out)


def prepare(project, output, domain, target, proposal_id, entrance=None,
            references=()):
    project, output = project.resolve(), output.resolve()
    if project.suffix != ".yazeproj" or not (project / "project.yaze").is_file():
        raise ValueError("Use an existing self-contained .yazeproj directory")
    if output.exists() or output.is_relative_to(project) or project.is_relative_to(output):
        raise ValueError("Output must be a new directory outside the source bundle")
    if domain not in ("overworld", "dungeon"):
        raise ValueError("Domain must be overworld or dungeon")
    if not 0 <= target < (192 if domain == "overworld" else 296):
        raise ValueError("Target is outside the supported map/room range")
    if domain == "dungeon" and (entrance is None or not 0 <= entrance < 140):
        raise ValueError("Dungeon proposals require an explicit graphics entrance slot (0..139)")
    if domain == "overworld" and entrance is not None:
        raise ValueError("Overworld proposals cannot select dungeon graphics entrances")
    original = inventory(project)
    config = descriptor(project / "project.yaze")
    rom = validate_paths(project, config)
    refs = [(p.resolve(), digest(p.resolve())) for p in references]
    # All source validation precedes output creation. A failed copy remains
    # available for inspection, but no proposal.json is published on failure.
    output.mkdir(parents=True)
    draft = output / "Draft.yazeproj"
    shutil.copytree(project, draft)
    if inventory(draft) != original or inventory(project) != original:
        raise ValueError("Bundle changed during copy; incomplete draft retained")
    history = output / "inherited-history"
    history.mkdir()
    receipt = draft / "project.yaze.native-save.json"
    if receipt.exists():
        receipt.rename(history / receipt.name)
    # A copied build is not the output of this draft's future real Save/Build.
    built = config.get("build", "build_target", fallback="")
    if built:
        rel = within(project, built).relative_to(project)
        for inherited in (rel, rel.with_suffix(".build.json"), Path(str(rel) + ".build.json")):
            old = draft / inherited
            if old.is_file():
                dest = history / inherited
                dest.parent.mkdir(parents=True, exist_ok=True)
                old.rename(dest)
    text = (draft / "project.yaze").read_text()
    # Relocate only known path fields, not dialogue, labels or shell source.
    changes = {}
    for section, keys in (("files", list(config["files"])),
                          ("build", ["output_folder", "build_target", "asm_entry_point", "git_repository"])):
        for key in keys:
            value = config.get(section, key, fallback="")
            if value and Path(value).is_absolute():
                changes[section, key] = str(draft / within(project, value).relative_to(project))
    changes["workspace", "recent_files"] = ""
    changes["planning", "file"] = ""  # Do not write the live shared plan.
    (draft / "project.yaze").write_text(replace_fields(text, changes))
    validate_paths(draft, descriptor(draft / "project.yaze"))
    baseline_rom = output / "baseline.rom"
    shutil.copy2(rom, baseline_rom)
    baseline = inventory(draft)
    write_json(output / "baseline.json", baseline)
    unbound = []
    for index, (path, sha) in enumerate(refs):
        dest = output / "references" / f"{index:03d}-{path.name}"
        dest.parent.mkdir(exist_ok=True)
        shutil.copy2(path, dest)
        unbound.append({"path": str(dest.relative_to(output)), "sha256": sha,
                        "state": "unbound-reference-only"})
    if inventory(project) != original:
        raise ValueError("Source changed during preparation; incomplete draft retained")
    packet = {"schema": SCHEMA, "proposal_id": proposal_id, "domain": domain,
              "target": target, "graphics_entrance_slot": entrance,
              "original_project": str(project), "original_inventory": original,
              "draft_project": "Draft.yazeproj", "rom": str(rom.relative_to(project)),
              "baseline_inventory_sha256": digest(output / "baseline.json"),
              "baseline_rom_sha256": digest(baseline_rom), "references": unbound,
              "proof": "prepared-only; normal Save/reopen/Build/Run not performed",
              "delivery": "private; contains ROM and source; not a public package"}
    write_json(output / "proposal.json", packet)
    return packet


def load_packet(root):
    root = root.resolve()
    data = json.loads((root / "proposal.json").read_text())
    if data.get("schema") != SCHEMA:
        raise ValueError("Unsupported proposal schema")
    for name, field in (("baseline.json", "baseline_inventory_sha256"),
                        ("baseline.rom", "baseline_rom_sha256")):
        if digest(root / name) != data[field]:
            raise ValueError(f"Proposal baseline changed: {name}")
    draft = within(root, data["draft_project"])
    validate_paths(draft, descriptor(draft / "project.yaze"))
    return data, draft


def launch_plan(root, executable):
    data, draft = load_packet(root)
    flags = [str(executable.resolve()), f"--proposal={root.resolve()}",
             f"--rom_file={draft / 'project.yaze'}",
             f"--editor={data['domain'].title()}", "--startup_welcome=hide",
             "--startup_dashboard=hide", "--startup_sidebar=hide"]
    if data["domain"] == "dungeon":
        flags += [f"--room={data['target']}",
                  f"--graphics_entrance={data['graphics_entrance_slot']}",
                  "--open_panels=dungeon.workbench,proposal.workbench"]
    else:
        flags += [f"--map={data['target']}", "--open_panels=proposal.workbench"]
    return {"argv": flags, "env": {"YAZE_APP_DATA_DIR": str(root.resolve() / "app-data")},
            "cwd": str(draft), "unbound_references": data["references"],
            "note": "Launch through the owned app/window host; no launch performed. "
                    "Reference images are not imported into the model. "
                    "Save normally before Build/Run."}


def byte_spans(before, after):
    """Coalesce contiguous byte changes; report offsets, never whole ROM data."""
    result, start = [], None
    for index in range(max(len(before), len(after))):
        changed = before[index:index + 1] != after[index:index + 1]
        if changed and start is None:
            start = index
        if start is not None and (not changed or index == max(len(before), len(after)) - 1):
            end = index if not changed else index + 1
            result.append({"offset": start, "length": end - start})
            start = None
    return result


def feedback(root):
    data, draft = load_packet(root)
    before = json.loads((root / "baseline.json").read_text())
    after = inventory(draft)
    files = [{"path": p, "before_sha256": before.get(p), "after_sha256": after.get(p)}
             for p in sorted(before.keys() | after.keys()) if before.get(p) != after.get(p)]
    spans = byte_spans((root / "baseline.rom").read_bytes(),
                      within(draft, data["rom"]).read_bytes())
    return {"schema": "yaze.proposal-feedback.v1", "proposal_id": data["proposal_id"],
            "domain": data["domain"], "target": data["target"],
            "graphics_entrance_slot": data["graphics_entrance_slot"],
            "baseline_rom_sha256": data["baseline_rom_sha256"],
            "edited_rom_sha256": digest(within(draft, data["rom"])),
            "changed_files": files, "rom_changed_spans": spans,
            "rom_changed_bytes": sum(s["length"] for s in spans),
            "unbound_references": data["references"],
            "semantic_diff": "pending domain exporter; byte changes do not prove room/map scope",
            "adoption": "never automatic; review revised draft against preserved baseline"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    prep = sub.add_parser("prepare")
    prep.add_argument("--project", type=Path, required=True)
    prep.add_argument("--out", type=Path, required=True)
    prep.add_argument("--domain", choices=("overworld", "dungeon"), required=True)
    prep.add_argument("--target", type=lambda x: int(x, 0), required=True)
    prep.add_argument("--proposal-id", required=True)
    prep.add_argument("--graphics-entrance", type=lambda x: int(x, 0))
    prep.add_argument("--reference", type=Path, action="append", default=[])
    launch = sub.add_parser("launch-plan")
    launch.add_argument("packet", type=Path)
    launch.add_argument("--yaze", type=Path, required=True)
    export = sub.add_parser("feedback")
    export.add_argument("packet", type=Path)
    for command in (launch, export):
        command.add_argument("--out", type=Path)
    args = parser.parse_args()
    try:
        if args.command == "prepare":
            result = prepare(args.project, args.out, args.domain, args.target,
                             args.proposal_id, args.graphics_entrance, args.reference)
        elif args.command == "launch-plan":
            result = launch_plan(args.packet, args.yaze)
        else:
            result = feedback(args.packet)
        if args.command != "prepare" and args.out:
            write_json(args.out, result)
        else:
            print(json.dumps(result, indent=2))
    except (ValueError, OSError, configparser.Error, KeyError) as error:
        parser.exit(2, f"proposal workbench: {error}\n")


if __name__ == "__main__":
    main()
