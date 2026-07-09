#!/usr/bin/env python3
"""Validate committed i18n catalogs (CI gate).

For each assets/i18n/<locale>.json: valid JSON object, no empty translation, and
printf %-specifiers preserved key->value (a mismatch makes ImGui::Text(tr(fmt),
...) read the wrong args -> crash/UB). Reuses i18n_common.format_specifiers.

Usage: check_catalog.py [catalog.json ...]   (defaults to assets/i18n/*.json)
Exit non-zero on any error.
"""
import glob
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from i18n_common import format_specifiers  # noqa: E402


def check(path):
    errors = []
    try:
        data = json.load(open(path, encoding="utf-8"))
    except (OSError, ValueError) as e:
        return [f"{path}: invalid JSON: {e}"]
    if not isinstance(data, dict):
        return [f"{path}: top level must be a JSON object"]
    for key, value in data.items():
        if not isinstance(value, str) or not value.strip():
            errors.append(f"{path}: empty translation for {key!r}")
            continue
        if format_specifiers(key) != format_specifiers(value):
            errors.append(
                f"{path}: format-specifier mismatch {key!r} "
                f"{format_specifiers(key)} -> {value!r} {format_specifiers(value)}")
    return errors


def main(argv):
    paths = argv or sorted(glob.glob(
        os.path.join(os.path.dirname(__file__), "..", "..",
                     "assets", "i18n", "*.json")))
    errors = [e for p in paths for e in check(p)]
    for e in errors:
        print("ERROR", e)
    n = len(paths)
    print(f"checked {n} catalog(s), {len(errors)} error(s)")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
