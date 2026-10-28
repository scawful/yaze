#!/usr/bin/env python3
"""Migrate ImDrawList calls to the Dear ImGui 1.93 argument order.

Dear ImGui 1.92.8 swapped the last two parameters of three ImDrawList calls:

    AddRect(p_min, p_max, col, rounding, flags, thickness)
        -> AddRect(p_min, p_max, col, rounding, thickness, flags)
    AddPolyline(points, count, col, flags, thickness)
        -> AddPolyline(points, count, col, thickness, flags)
    PathStroke(col, flags, thickness)
        -> PathStroke(col, thickness, flags)

The old order still compiles through inline redirects unless
IMGUI_DISABLE_OBSOLETE_FUNCTIONS is defined, and calls whose arguments are all
integer literals (AddRect(a, b, c, 0, 0, 2)) are ambiguous and fail to build.
This script rewrites old-order member calls (`->AddRect(` or `.AddRect(`) and
drops a flags argument of `0` or `ImDrawFlags_None`.

Calls already in the new order are left alone, so the script can be rerun.
It also reports short calls that already bind a flag as the thickness, such as
`AddRect(a, b, c, r, ImDrawFlags_RoundCornersTop)`; those compile silently
under 1.93 and need a manual fix.

Usage:
    scripts/imgui_drawlist_migrate.py --check src test
    scripts/imgui_drawlist_migrate.py --write src/app/gui
    scripts/imgui_drawlist_migrate.py --write src --exclude 'src/app/editor/overworld/*'

With --axis-lines it instead rewrites axis-aligned AddLine() calls. 1.93
dropped AddLine's +0.5 offset, so a 1px line at integer coordinates now
straddles two pixels and draws blurred. AddLineH()/AddLineV() stroke inside
the given row or column, which matches the old AddLine output for thickness 1:

    AddLine(ImVec2(x, y1), ImVec2(x, y2), col) -> AddLineV(x, y1, y2, col)
    AddLine(ImVec2(x1, y), ImVec2(x2, y), col) -> AddLineH(x1, x2, y, col)

Only calls whose endpoints are ImVec2(...) expressions with a textually equal
x or y are rewritten. Calls with a literal thickness above 1 are kept: the new
centered AddLine already draws those crisply at integer coordinates.

Usage:
    scripts/imgui_drawlist_migrate.py --check --axis-lines src
    scripts/imgui_drawlist_migrate.py --write --axis-lines src/app/gui

Run `git clang-format` afterward; rewritten calls keep their old line breaks.
"""

import argparse
import fnmatch
import os
import re
import sys

# name -> (old argument count, index of the flags argument)
OLD_FORMS = {
    "AddRect": (6, 4),
    "AddPolyline": (5, 3),
    "PathStroke": (3, 1),
}
# name -> argument count of a new-order call that has no flags argument
SHORT_FORMS = {"AddRect": 5, "PathStroke": 2}

CALL_RE = re.compile(r"(?:->|\.)\s*(AddRect|AddPolyline|PathStroke)\s*\(")
FLAG_RE = re.compile(r"ImDrawFlags_|\bflags\b|Flags\b")
NO_FLAGS = {"0", "ImDrawFlags_None"}
INT_LITERAL_RE = re.compile(r"\d+")
FLOAT_LITERAL_RE = re.compile(r"(\d+\.?\d*|\.\d+)[fF]?")
LINE_CALL_RE = re.compile(r"(?:->|\.)\s*(AddLine)\s*\(")
IMVEC2_RE = re.compile(r"\s*ImVec2\s*\(")
SOURCE_EXTS = (".cc", ".cpp", ".h", ".hpp", ".mm", ".inl")
# newline="" keeps CRLF and mixed line endings byte-for-byte.
OPEN_ARGS = {"encoding": "utf-8", "errors": "surrogateescape", "newline": ""}


def split_args(text, start):
    """Splits a call's arguments starting just after its '('.

    Returns (segments, end) where each segment is the raw text between
    top-level commas, and end is the index of the closing ')'. Returns
    (None, start) when the call is not closed.
    """
    depth = 0
    segments = []
    seg_start = start
    i = start
    while i < len(text):
        c = text[i]
        if c in "\"'":
            quote = c
            i += 1
            while i < len(text) and text[i] != quote:
                i += 2 if text[i] == "\\" else 1
        elif text.startswith("//", i):
            i = text.find("\n", i)
            if i < 0:
                return None, start
        elif text.startswith("/*", i):
            i = text.find("*/", i)
            if i < 0:
                return None, start
            i += 1
        elif c in "([{":
            depth += 1
        elif c in ")]}":
            if depth == 0:
                segments.append(text[seg_start:i])
                return segments, i
            depth -= 1
        elif c == "," and depth == 0:
            segments.append(text[seg_start:i])
            seg_start = i + 1
        i += 1
    return None, start


def replace_value(segment, value):
    """Replaces a segment's content while keeping its leading whitespace."""
    stripped = segment.lstrip()
    prefix = segment[: len(segment) - len(stripped)]
    return prefix + value


def as_float_literal(value):
    return value + ".0f" if INT_LITERAL_RE.fullmatch(value) else value


def is_flag_like(value):
    return value in NO_FLAGS or bool(FLAG_RE.search(value))


def is_thickness_like(value):
    float_literal = FLOAT_LITERAL_RE.fullmatch(value) and not INT_LITERAL_RE.fullmatch(
        value
    )
    return bool(float_literal) or "thick" in value.lower()


def call_order(name, segments):
    """Returns "old", "new", or None when a full-length call's order is unclear.

    A new-order call that passes flags has the same argument count as an
    old-order call, so the order is read from the last two arguments.
    """
    _, flags_index = OLD_FORMS[name]
    slot = segments[flags_index].strip()
    last = segments[flags_index + 1].strip()
    if is_flag_like(slot) and not is_flag_like(last):
        return "old"
    if is_flag_like(last) or is_thickness_like(slot):
        return "new"
    return None


def migrate_call(name, segments):
    """Returns new argument segments for an old-order call."""
    _, flags_index = OLD_FORMS[name]
    flags = segments[flags_index].strip()
    thickness = segments[flags_index + 1].strip()
    out = list(segments[:flags_index])
    # AddRect's rounding and thickness are floats; integer literals there are
    # what made the old call ambiguous, so spell them as floats.
    if name == "AddRect":
        out[3] = replace_value(out[3], as_float_literal(out[3].strip()))
    out.append(replace_value(segments[flags_index], as_float_literal(thickness)))
    if flags not in NO_FLAGS:
        out.append(replace_value(segments[flags_index + 1], flags))
    return out


def parse_imvec2(segment):
    """Returns the (x, y) argument text of an `ImVec2(x, y)` segment, or None."""
    m = IMVEC2_RE.match(segment)
    if not m:
        return None
    parts, end = split_args(segment, m.end())
    if parts is None or len(parts) != 2 or segment[end + 1 :].strip():
        return None
    return parts[0].strip(), parts[1].strip()


def migrate_axis_line(segments):
    """Returns (name, args) for an axis-aligned AddLine call, or None."""
    if len(segments) not in (3, 4):
        return None
    a = parse_imvec2(segments[0])
    b = parse_imvec2(segments[1])
    if a is None or b is None:
        return None
    norm = lambda v: re.sub(r"\s+", "", v)
    if norm(a[0]) == norm(b[0]) and norm(a[1]) != norm(b[1]):
        name, args = "AddLineV", [a[0], a[1], b[1]]
    elif norm(a[1]) == norm(b[1]) and norm(a[0]) != norm(b[0]):
        name, args = "AddLineH", [a[0], b[0], a[1]]
    else:
        return None
    args.append(segments[2].strip())
    if len(segments) == 4:
        thickness = segments[3].strip()
        if FLOAT_LITERAL_RE.fullmatch(thickness):
            value = float(thickness.rstrip("fF"))
            if value > 1.0:
                return None
            if value != 1.0:
                args.append(thickness)
        else:
            args.append(thickness)
    return name, args


def process_axis_lines(path, write):
    with open(path, **OPEN_ARGS) as f:
        text = f.read()
    edits = []
    for m in LINE_CALL_RE.finditer(text):
        segments, end = split_args(text, m.end())
        if segments is None:
            continue
        result = migrate_axis_line(segments)
        if result is None:
            continue
        name, args = result
        line = text.count("\n", 0, m.start()) + 1
        edits.append((m.start(1), end, f"{name}({', '.join(args)}", line))
    if write and edits:
        for start, end, new_call, _ in reversed(edits):
            text = text[:start] + new_call + text[end:]
        with open(path, "w", **OPEN_ARGS) as f:
            f.write(text)
    return [line for *_, line in edits], []


def process_file(path, write):
    with open(path, **OPEN_ARGS) as f:
        text = f.read()
    edits = []
    warnings = []
    for m in CALL_RE.finditer(text):
        name = m.group(1)
        segments, end = split_args(text, m.end())
        if segments is None:
            continue
        line = text.count("\n", 0, m.start()) + 1
        old_count, _ = OLD_FORMS[name]
        if len(segments) == old_count:
            order = call_order(name, segments)
            if order == "old":
                new_args = ",".join(migrate_call(name, segments))
                edits.append((m.end(), end, new_args, line))
            elif order is None:
                last_two = ", ".join(seg.strip() for seg in segments[-2:])
                warnings.append((line, name, f"unclear order: {last_two}"))
        elif SHORT_FORMS.get(name) == len(segments):
            last = segments[-1].strip()
            if is_flag_like(last) and last not in NO_FLAGS:
                warnings.append((line, name, last))
    if write and edits:
        for start, end, new_args, _ in reversed(edits):
            text = text[:start] + new_args + text[end:]
        with open(path, "w", **OPEN_ARGS) as f:
            f.write(text)
    return [line for *_, line in edits], warnings


def iter_files(paths, excludes):
    for root in paths:
        if os.path.isfile(root):
            candidates = [root]
        else:
            candidates = (
                os.path.join(dp, f) for dp, _, fs in os.walk(root) for f in fs
            )
        for path in candidates:
            norm = os.path.normpath(path)
            if not norm.endswith(SOURCE_EXTS):
                continue
            if any(fnmatch.fnmatch(norm, pat) for pat in excludes):
                continue
            yield norm


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check", action="store_true", help="report only")
    mode.add_argument("--write", action="store_true", help="rewrite files")
    parser.add_argument(
        "--axis-lines",
        action="store_true",
        help="rewrite axis-aligned AddLine calls to AddLineH/AddLineV",
    )
    parser.add_argument("paths", nargs="+")
    parser.add_argument(
        "--exclude", action="append", default=[], help="fnmatch glob to skip"
    )
    args = parser.parse_args()

    total = 0
    warned = 0
    for path in sorted(iter_files(args.paths, args.exclude)):
        process = process_axis_lines if args.axis_lines else process_file
        lines, warnings = process(path, args.write)
        found = "axis-aligned AddLine" if args.axis_lines else "old order"
        for line in lines:
            print(f"{path}:{line}: {'migrated' if args.write else found}")
        for line, name, detail in warnings:
            if detail.startswith("unclear order"):
                print(f"{path}:{line}: warning: {name} {detail}")
            else:
                print(f"{path}:{line}: warning: {name} passes '{detail}' as thickness")
        total += len(lines)
        warned += len(warnings)
    verb = "migrated" if args.write else "found"
    kind = "axis-aligned AddLine" if args.axis_lines else "old-order"
    print(f"{verb} {total} {kind} call(s); {warned} warning(s)", file=sys.stderr)
    return 1 if (args.check and total) or warned else 0


if __name__ == "__main__":
    sys.exit(main())
