#!/usr/bin/env python3
"""CI guard: fail if a changed file adds user-facing text NOT wrapped in tr().

Reuses wrap.py's EXACT detection (wrap.find_wrap_edits): a finding here is
precisely a literal the wrap.py codemod would wrap - a bare string-literal first
arg to a LABEL_FIRST_CALLS call (or the 2nd arg of TextColored) that has real
alphabetic content (i18n_common.visible_key) and is not already inside tr(). By
delegating to find_wrap_edits the finding set can never diverge from the codemod,
so this introduces no new false positives.

This is a "no NEW bare text" regression gate. Files the codemod deliberately
skips (agent/AI UI, MenuBuilder choke points whose labels are translated at
draw time, and the vendored text_editor) are excluded here too via
wrap.excluded().

Two modes:
  check_wrapped.py [FILE ...]              scan whole files (standalone)
  check_wrapped.py --diff BASE HEAD [FILE ...]
      Only report findings on lines ADDED/MODIFIED between BASE and HEAD (per
      `git diff -U0 BASE HEAD -- FILE`). Pre-existing unwrapped literals no
      longer fail CI; only newly added ones do.

Exit 1 if any (in-scope) unwrapped literal is found, else 0.
"""
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import i18n_common as ic  # noqa: E402
import wrap  # noqa: E402


def _call_name(src, lit_start):
    """Best-effort name of the call whose argument literal begins at lit_start.
    Handles first-arg calls and TextColored's 2nd arg (skips back past the
    color expression to the enclosing '(')."""
    i = lit_start - 1
    while i >= 0 and src[i] in " \t\r\n":
        i -= 1
    if i >= 0 and src[i] == ",":            # 2nd-arg call (e.g. TextColored)
        depth = 0
        while i >= 0:
            c = src[i]
            if c in ")]}":
                depth += 1
            elif c in "([{":
                if depth == 0:
                    break
                depth -= 1
            i -= 1
        i -= 1                              # step before the enclosing '('
    elif i >= 0 and src[i] == "(":          # first-arg call
        i -= 1
    while i >= 0 and src[i] in " \t\r\n":
        i -= 1
    end = i + 1
    while i >= 0 and (src[i].isalnum() or src[i] == "_"):
        i -= 1
    return src[i + 1:end] or "?"


def _findings(path):
    """List of (line_number, message) for every in-scope unwrapped literal."""
    if not path.endswith(wrap.EXTS) or wrap.excluded(path):
        return []
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as fh:
            src = ic.strip_comments(fh.read())
    except OSError:
        return []
    out = []
    for lit_start, _end, _lit in wrap.find_wrap_edits(src)[0]:
        line = src.count("\n", 0, lit_start) + 1
        value, _ = ic.read_adjacent_literals(src, lit_start)
        text = (ic.visible_key(value) or value).replace("\n", "\\n").replace(
            "\r", "")
        name = _call_name(src, lit_start)
        out.append((line, f'{path}:{line}: unwrapped: {name}("{text}")'))
    return out


def check_file(path):
    return [msg for _line, msg in _findings(path)]


_HUNK = re.compile(r"^@@ -\d+(?:,\d+)? \+(\d+)(?:,(\d+))? @@")


def added_lines(base, head, path):
    """Set of +side line numbers added/modified in `path` between base..head."""
    try:
        diff = subprocess.run(
            ["git", "diff", "-U0", base, head, "--", path],
            capture_output=True, text=True, check=True).stdout
    except (subprocess.CalledProcessError, OSError):
        return set()
    added = set()
    for m in (_HUNK.match(ln) for ln in diff.splitlines()):
        if not m:
            continue
        start = int(m.group(1))
        count = 1 if m.group(2) is None else int(m.group(2))
        added.update(range(start, start + count))  # count==0 -> nothing
    return added


def main(argv):
    diff_mode = argv[:1] == ["--diff"]
    if diff_mode:
        if len(argv) < 3:
            print("usage: check_wrapped.py --diff BASE HEAD [FILE ...]")
            return 2
        base, head, files = argv[1], argv[2], argv[3:]
    else:
        base = head = None
        files = argv

    if not files:
        print("check_wrapped: no files")
        return 0

    findings = []
    for p in files:
        found = _findings(p)
        if diff_mode:
            added = added_lines(base, head, p)
            found = [f for f in found if f[0] in added]
        findings.extend(msg for _line, msg in found)

    for f in findings:
        print(f)
    where = "on added lines of" if diff_mode else "across"
    print(f"check_wrapped: {len(findings)} unwrapped literal(s) "
          f"{where} {len(files)} file(s)")
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
