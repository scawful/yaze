#!/usr/bin/env python3
"""Harvest translatable strings (msgids) and merge them into a locale catalog.

Scans src/app/{editor,gui,emu} for:
  * tr("...")            - the primary wrapped-string convention (incl. adjacent
                           literal concatenation and u8"" prefixes)
  * MenuBuilder labels   - .Item("..."), .BeginMenu("..."), .BeginSubMenu("..."),
                           .DisabledItem("..."), .CustomMenu("...") which are
                           translated at draw time (not wrapped in tr()).

msgids are the VISIBLE text only (ImGui "##id" suffixes stripped). Pure
format/punctuation labels are ignored.

Merges into assets/i18n/<locale>.json:
  * keeps existing translations for keys still present,
  * adds new keys with an empty "" value (fall back to source at runtime),
  * moves keys no longer present to <locale>.orphans.json (never silently lost),
  * lints that every non-empty translation preserves the msgid's %-specifiers
    in order (mismatch -> written to <locale>.fmt_errors.txt and exit code 2).

Usage: python3 scripts/i18n/extract.py [--locale fr] [--check]
  --check : do not write; exit non-zero if catalog would change or lint fails.
"""

import argparse
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import i18n_common as ic  # noqa: E402

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SCAN_DIRS = ["src/app/editor", "src/app/gui", "src/app/emu"]
EXTS = (".cc", ".h", ".mm", ".cpp", ".hpp")


def iter_source_files():
    for rel in SCAN_DIRS:
        base = os.path.join(REPO, rel)
        for root, _dirs, files in os.walk(base):
            for f in files:
                if f.endswith(EXTS):
                    yield os.path.join(root, f)


def harvest_file(path, keys, dynamic):
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        src = ic.strip_comments(fh.read())

    # tr( ... )
    for m in re.finditer(r"\btr\s*\(", src):
        value, _end = ic.read_adjacent_literals(src, m.end())
        if value is None:
            dynamic.append((path, m.start()))
            continue
        k = ic.visible_key(value)
        if k:
            keys.add(k)

    # MenuBuilder fluent labels: .Method("literal"
    for method in ic.MENU_BUILDER_CALLS:
        for m in re.finditer(r"\.\s*" + method + r"\s*\(", src):
            value, _end = ic.read_adjacent_literals(src, m.end())
            if value is None:
                continue
            k = ic.visible_key(value)
            if k:
                keys.add(k)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--locale", default="fr")
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()

    keys = set()
    dynamic = []
    for path in iter_source_files():
        harvest_file(path, keys, dynamic)

    catalog_path = os.path.join(REPO, "assets", "i18n", args.locale + ".json")
    existing = {}
    if os.path.exists(catalog_path):
        with open(catalog_path, "r", encoding="utf-8") as fh:
            existing = json.load(fh)

    merged = {}
    for k in sorted(keys):
        merged[k] = existing.get(k, "")

    orphans = {k: v for k, v in existing.items() if k not in keys and v}

    # Format-specifier lint on non-empty translations.
    fmt_errors = []
    for k, v in merged.items():
        if not v:
            continue
        if ic.format_specifiers(k) != ic.format_specifiers(v):
            fmt_errors.append(k)

    translated = sum(1 for v in merged.values() if v)
    print(f"[extract] locale={args.locale} msgids={len(merged)} "
          f"translated={translated} untranslated={len(merged) - translated} "
          f"orphans={len(orphans)} dynamic_calls={len(dynamic)} "
          f"fmt_errors={len(fmt_errors)}")

    if fmt_errors:
        errp = os.path.join(REPO, "assets", "i18n",
                            args.locale + ".fmt_errors.txt")
        with open(errp, "w", encoding="utf-8") as fh:
            fh.write("\n".join(fmt_errors) + "\n")
        print(f"[extract] FORMAT ERRORS written to {errp}", file=sys.stderr)

    would_change = (merged != existing)
    if args.check:
        if fmt_errors or would_change:
            print("[extract] --check failed (catalog stale or fmt errors)",
                  file=sys.stderr)
            return 2
        return 0

    with open(catalog_path, "w", encoding="utf-8") as fh:
        json.dump(merged, fh, ensure_ascii=False, indent=2, sort_keys=True)
        fh.write("\n")
    if orphans:
        orphp = os.path.join(REPO, "assets", "i18n",
                             args.locale + ".orphans.json")
        with open(orphp, "w", encoding="utf-8") as fh:
            json.dump(orphans, fh, ensure_ascii=False, indent=2, sort_keys=True)
            fh.write("\n")
    if dynamic:
        dynp = os.path.join(REPO, "assets", "i18n", "dynamic_tr_calls.txt")
        with open(dynp, "w", encoding="utf-8") as fh:
            for p, off in dynamic:
                fh.write(f"{os.path.relpath(p, REPO)}\t@{off}\n")
    return 2 if fmt_errors else 0


if __name__ == "__main__":
    sys.exit(main())
