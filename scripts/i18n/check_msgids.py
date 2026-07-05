#!/usr/bin/env python3
"""Informational i18n coverage report - NOT a CI gate (ALWAYS exits 0).

Reuses extract.py's harvester (extract.iter_source_files + extract.harvest_file)
to enumerate every source msgid - the literal keys wrapped in tr() plus the
MenuBuilder labels translated at draw time, i.e. exactly the keys extract.py would
put in the catalog - across src/, then compares them to assets/i18n/<locale>.json:

  * MISSING = source msgid present in code but absent from the catalog
    (untranslated). A nonzero count is EXPECTED: ~135 wrapped header/section
    strings are intentionally left untranslated. This must NOT fail the build.
  * ORPHAN  = catalog key that matches no source literal. This has false
    positives: tr(variable) data-table indirection produces runtime keys that
    have no source literal, alongside genuinely stale entries. Reported, never
    enforced.

Because both counts are inherently noisy, this script is a coverage *report*, not
a gate - its exit code is ALWAYS 0. The real catalog gate is check_catalog.py.

Usage: python3 scripts/i18n/check_msgids.py [--locale fr]
"""
import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import extract  # noqa: E402


def source_msgids():
    keys = set()
    dynamic = []
    for path in extract.iter_source_files():
        extract.harvest_file(path, keys, dynamic)
    return keys, dynamic


def _sample(title, items, limit=15):
    print(f"  {title}: {len(items)}")
    for k in items[:limit]:
        print(f"    - {k!r}")
    if len(items) > limit:
        print(f"    ... and {len(items) - limit} more")


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("--locale", default="fr")
    args = ap.parse_args(argv)

    keys, dynamic = source_msgids()

    catalog_path = os.path.join(extract.REPO, "assets", "i18n",
                                args.locale + ".json")
    catalog = {}
    if os.path.exists(catalog_path):
        with open(catalog_path, "r", encoding="utf-8") as fh:
            catalog = json.load(fh)
    cat_keys = set(catalog)

    missing = sorted(keys - cat_keys)
    orphan = sorted(cat_keys - keys)
    covered = len(keys & cat_keys)
    pct = (100.0 * covered / len(keys)) if keys else 100.0

    print(f"[i18n coverage] locale={args.locale}  (report only, exit 0)")
    print(f"  source msgids (tr() + MenuBuilder): {len(keys)}")
    print(f"  catalogue entries                 : {len(cat_keys)}")
    print(f"  covered (source msgid has entry)  : {covered}  ({pct:.1f}%)")
    print(f"  tr(variable) dynamic calls        : {len(dynamic)}")
    print()
    _sample("MISSING (source msgid, no catalog entry -> untranslated)", missing)
    print()
    _sample("ORPHAN (catalog key, no source literal -> stale or tr(var))",
            orphan)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
