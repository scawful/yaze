#!/usr/bin/env python3
"""Package verified indexed art kits for the existing Graphics Catalog reader."""

import argparse
import hashlib
import json
from pathlib import Path
import shutil


def package(source: Path, output: Path) -> None:
    library = json.loads((source / "library.json").read_text())
    imports = json.loads((source / "import-sheets/import-index.json").read_text())
    kits = {entry["id"]: entry for entry in library["assets"]}
    references = []
    output.mkdir(parents=True, exist_ok=True)
    for entry in imports["assets"]:
        kit = json.loads((source / kits[entry["id"]]["path"]).read_text())
        directory = output / "pages" / entry["id"].split(".", 1)[-1]
        directory.mkdir(parents=True, exist_ok=True)
        pages = []
        for page_path in entry["page_files"]:
            original = source / "import-sheets" / page_path
            destination = directory / original.name
            shutil.copyfile(original, destination)
            pages.append({
                "path": destination.relative_to(output).as_posix(),
                "sha256": hashlib.sha256(destination.read_bytes()).hexdigest(),
            })
        source_preview = (source / kits[entry["id"]]["path"]).parent / "source.png"
        preview = directory / "source.png"
        shutil.copyfile(source_preview, preview)
        provenance = kit["source"]
        material = provenance["semantic_palette_family"]
        references.append({
            "id": entry["id"],
            "title": kit["name"],
            "name": kit["name"],
            "role": "source-sheet",
            "source_kind": "indexed-png",
            "bpp": 3,
            "group": material.rsplit("-", 1)[0].replace("-", " ").title(),
            "category": kit["category"],
            "material": material,
            "game": kit["game_family"],
            "credit": provenance.get("source_credit", "Source credit unavailable"),
            "source_sha256": provenance["source_sha256"],
            "source_origin": Path(provenance["source_path"]).name,
            "source_rect": provenance["rect"],
            "pages": pages,
            "palette_bgr555": [int(word, 16) for word in kit["variants"]["bg3"]["palette_bgr555"]],
            "pixel_size": kit["padded_pixel_size"],
            "assembly": [
                {"x": cell["x"], "y": cell["y"], "page": cell["import_page"],
                 "page_tile": cell["page_tile"], "h": cell["h"], "v": cell["v"]}
                for cell in entry["assembly"]
            ],
            "preview_path": preview.relative_to(output).as_posix(),
            "preview_sha256": hashlib.sha256(preview.read_bytes()).hexdigest(),
            "resources_bound": False,
        })
    manifest = {
        "schema_version": 1,
        "title": "Minish Cap indoor art",
        "references": references,
    }
    (output / "catalog.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Packaged {len(references)} assets / {sum(len(r['pages']) for r in references)} indexed pages")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="Verified kit root with library.json and import-sheets/")
    parser.add_argument("output", type=Path, help="Catalog resource directory")
    options = parser.parse_args()
    package(options.source.resolve(), options.output.resolve())
