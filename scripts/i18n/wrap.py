#!/usr/bin/env python3
"""Conservative codemod: wrap user-visible ImGui string literals in tr().

Transforms e.g.  ImGui::Button("Clear")  ->  ImGui::Button(tr("Clear"))
and inserts  #include "util/i18n/tr.h"  once per modified file.

SKIPS (never wraps): non-literal args (variables/concatenation with vars),
already-wrapped tr(...) args, pure format/punctuation/id labels ("%d", "->",
"##id"), and comments. TextColored wraps its 2nd (format) argument.

Whole subtrees excluded per the end-user scope: agent/AI, emulator internals,
and the MenuBuilder choke-point files (labels there are translated at draw).

Usage:
  python3 scripts/i18n/wrap.py [PATHS...] [--apply]
    PATHS default to the end-user UI dirs. Without --apply it's a dry run
    (prints per-file counts and the skipped-ambiguous review list).
"""

import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import i18n_common as ic  # noqa: E402

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_PATHS = ["src/app/editor", "src/app/gui"]
EXTS = (".cc", ".mm", ".cpp", ".h", ".hpp")  # impl files and header-only UI

# Files/dirs excluded from wrapping (end-user scope + choke points).
EXCLUDE_SUBSTR = [
    os.sep + "agent" + os.sep,        # AI/agent UI (dev-facing)
    os.sep + "menu" + os.sep + "menu_builder",
    os.sep + "menu" + os.sep + "menu_orchestrator",
    "widgets" + os.sep + "text_editor",  # vendored code editor (no yaze ns)
]
INCLUDE_LINE = '#include "util/i18n/tr.h"'


def skip_one_arg(src, pos):
    """From the start of an argument, returns the index just AFTER the
    top-level comma that ends it (or the matching ')' index if it's the last
    arg). Respects nested (), [], {} and string/char literals."""
    n = len(src)
    depth = 0
    i = pos
    while i < n:
        c = src[i]
        if c == '"':
            i = ic._end_of_string(src, i)
            continue
        if c == "'":
            i += 1
            while i < n and src[i] != "'":
                if src[i] == "\\":
                    i += 1
                i += 1
            i += 1
            continue
        if c in "([{":
            depth += 1
        elif c in ")]}":
            if depth == 0:
                return i  # end of the call's arg list
            depth -= 1
        elif c == "," and depth == 0:
            return i + 1
        i += 1
    return n


def find_wrap_edits(src):
    """Returns a list of (start, end, literal_text) spans to wrap, plus a list
    of (offset) of ambiguous skipped call sites for review."""
    edits = []
    review = []

    calls = [(name, 0) for name in ic.LABEL_FIRST_CALLS]
    calls += [("TextColored", 1)]

    for name, argidx in calls:
        # Match `Name(` as ImGui::Name(, gui::Name(, or bare Name( - but not as
        # a substring of a longer identifier, and not a member `.Name(`.
        for m in re.finditer(r"(?<![A-Za-z0-9_.])" + name + r"\s*\(", src):
            arg_start = m.end()
            if argidx == 1:
                arg_start = skip_one_arg(src, arg_start)
            # skip whitespace
            j = arg_start
            while j < len(src) and src[j] in " \t\r\n":
                j += 1
            value, end = ic.read_adjacent_literals(src, j)
            if value is None:
                # not a plain literal (variable / tr(...) / expression)
                # only flag as review if it's clearly text-ish (heuristic: next
                # token is an identifier, not ')').
                if j < len(src) and src[j] not in ")":
                    review.append(m.start())
                continue
            if ic.visible_key(value) is None:
                continue  # format-only / id-only, leave as-is
            lit_start = j
            edits.append((lit_start, end, src[lit_start:end]))
    # de-dup / sort by position
    edits.sort()
    dedup = []
    last_end = -1
    for s, e, t in edits:
        if s >= last_end:
            dedup.append((s, e, t))
            last_end = e
    return dedup, review


def process_file(path, apply):
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        original = fh.read()
    stripped = ic.strip_comments(original)  # same length => offsets align
    edits, review = find_wrap_edits(stripped)
    if not edits:
        return 0, review
    # Apply from end to start on the ORIGINAL text (literals identical in both).
    new = original
    for s, e, lit in reversed(edits):
        new = new[:s] + "tr(" + lit + ")" + new[e:]
    if INCLUDE_LINE not in new:
        new = insert_include(new)
    if apply:
        with open(path, "w", encoding="utf-8") as fh:
            fh.write(new)
    return len(edits), review


def insert_include(src):
    """Inserts the tr.h include after the FIRST top-level #include line. Using
    the first (not last) include avoids landing inside a trailing #ifdef block
    (e.g. an optional-feature include guarded by #ifdef), which would make the
    include conditional and leave tr undeclared in default builds."""
    lines = src.split("\n")
    for i, ln in enumerate(lines):
        if ln.startswith("#include"):
            lines.insert(i + 1, INCLUDE_LINE)
            return "\n".join(lines)
    return INCLUDE_LINE + "\n" + src


def excluded(path):
    return any(sub in path for sub in EXCLUDE_SUBSTR)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("paths", nargs="*", default=DEFAULT_PATHS)
    ap.add_argument("--apply", action="store_true")
    args = ap.parse_args()

    total_edits = 0
    total_files = 0
    review_all = []
    for rel in args.paths:
        base = os.path.join(REPO, rel)
        for root, _dirs, files in os.walk(base):
            for f in sorted(files):
                if not f.endswith(EXTS):
                    continue
                path = os.path.join(root, f)
                if excluded(path):
                    continue
                count, review = process_file(path, args.apply)
                if count:
                    total_edits += count
                    total_files += 1
                    print(f"  {'wrapped' if args.apply else 'would wrap'} "
                          f"{count:4d} in {os.path.relpath(path, REPO)}")
                for off in review:
                    review_all.append((path, off))

    print(f"[wrap] files={total_files} edits={total_edits} "
          f"review={len(review_all)} apply={args.apply}")
    if review_all:
        revp = os.path.join(REPO, "assets", "i18n", "wrap.review.txt")
        os.makedirs(os.path.dirname(revp), exist_ok=True)
        with open(revp, "w", encoding="utf-8") as fh:
            for p, off in review_all:
                fh.write(f"{os.path.relpath(p, REPO)}\t@{off}\n")
        print(f"[wrap] ambiguous sites -> {os.path.relpath(revp, REPO)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
