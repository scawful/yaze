#!/usr/bin/env python3
"""Merge translated part files (parts/fr.part_*.json) into the locale catalog.

Only keys that already exist in the catalog are updated (guards against stray
keys), and only non-empty translations are applied. Prints a coverage summary.

Usage: python3 scripts/i18n/merge_parts.py [--locale fr]
"""

import argparse
import glob
import json
import os
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--locale", default="fr")
    args = ap.parse_args()

    catalog_path = os.path.join(REPO, "assets", "i18n", args.locale + ".json")
    with open(catalog_path, "r", encoding="utf-8") as fh:
        catalog = json.load(fh)

    parts_dir = os.path.join(REPO, "scripts", "i18n", "parts")
    applied = 0
    unknown = 0
    for part in sorted(glob.glob(os.path.join(parts_dir, "*.json"))):
        with open(part, "r", encoding="utf-8") as fh:
            data = json.load(fh)
        for k, v in data.items():
            if not v:
                continue
            if k in catalog:
                if not catalog[k]:
                    applied += 1
                catalog[k] = v
            else:
                unknown += 1

    with open(catalog_path, "w", encoding="utf-8") as fh:
        json.dump(catalog, fh, ensure_ascii=False, indent=2, sort_keys=True)
        fh.write("\n")

    translated = sum(1 for v in catalog.values() if v)
    total = len(catalog)
    print(f"[merge] applied={applied} unknown_keys_skipped={unknown} "
          f"coverage={translated}/{total} "
          f"({100 * translated // max(total, 1)}%)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
