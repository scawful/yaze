"""Shared helpers for the yaze i18n tooling (stdlib only).

Provides a small C++ scanner used by both extract.py (harvest msgids) and
wrap.py (codemod). It understands string literals, adjacent-literal
concatenation, line/block comments, and the ImGui "##id"/"###id" convention.
"""

import re

# Calls whose FIRST string-literal argument is user-visible text.
LABEL_FIRST_CALLS = [
    "Text", "TextUnformatted", "TextWrapped", "TextDisabled", "BulletText",
    "LabelText", "Button", "SmallButton", "MenuItem", "Selectable",
    "CollapsingHeader", "SetTooltip", "SetItemTooltip", "BeginTabItem",
    "Checkbox", "RadioButton", "BeginMenu", "BeginCombo",
    "InputText", "InputTextMultiline", "InputInt", "InputFloat",
    "InputDouble", "SliderInt", "SliderFloat", "DragInt", "DragFloat", "Combo",
    "SeparatorText",
]
# NOTE: OpenPopup/BeginPopup*/TreeNode* are intentionally excluded: their string
# argument doubles as an ImGui id used to MATCH across calls (OpenPopup id must
# equal BeginPopup id) or persist tree state, so translating it in place breaks
# behavior. These are handled by agent review with explicit "##id" suffixes.

# Calls whose SECOND argument is the format/text (first arg is a color/other).
LABEL_SECOND_CALLS = ["TextColored", "TextWrapped"]  # TextWrapped is first-arg; kept simple below

# MenuBuilder fluent methods: first string literal is the label (translated at
# draw time by MenuBuilder, so wrap.py must NOT wrap these, but extract.py must
# still harvest them as msgids).
MENU_BUILDER_CALLS = ["Item", "BeginMenu", "BeginSubMenu", "DisabledItem",
                      "CustomMenu"]

_IDENT = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")


def strip_comments(src):
    """Returns src with // and /* */ comments blanked (length preserved)."""
    out = []
    i, n = 0, len(src)
    while i < n:
        c = src[i]
        if c == '"':
            j = _end_of_string(src, i)
            out.append(src[i:j])
            i = j
            continue
        if c == "'":
            j = i + 1
            while j < n and src[j] != "'":
                if src[j] == "\\":
                    j += 1
                j += 1
            out.append(src[i:j + 1])
            i = j + 1
            continue
        if c == "/" and i + 1 < n and src[i + 1] == "/":
            while i < n and src[i] != "\n":
                out.append(" ")
                i += 1
            continue
        if c == "/" and i + 1 < n and src[i + 1] == "*":
            while i < n and not (src[i] == "*" and i + 1 < n and src[i + 1] == "/"):
                out.append("\n" if src[i] == "\n" else " ")
                i += 1
            out.append("  ")
            i += 2
            continue
        out.append(c)
        i += 1
    return "".join(out)


def _end_of_string(src, i):
    """Given src[i] == '"', returns index just past the closing quote."""
    n = len(src)
    j = i + 1
    while j < n:
        if src[j] == "\\":
            j += 2
            continue
        if src[j] == '"':
            return j + 1
        j += 1
    return n


def decode_cpp_string(lit):
    """Decodes a single C++ double-quoted literal body (with surrounding quotes,
    optional u8 prefix) to its runtime string value."""
    if lit.startswith("u8"):
        lit = lit[2:]
    assert lit and lit[0] == '"'
    body = lit[1:-1]
    out = []
    i, n = 0, len(body)
    while i < n:
        c = body[i]
        if c != "\\":
            out.append(c)
            i += 1
            continue
        i += 1
        e = body[i] if i < n else ""
        i += 1
        simple = {"n": "\n", "t": "\t", "r": "\r", '"': '"', "\\": "\\",
                  "/": "/", "b": "\b", "f": "\f", "0": "\0", "a": "\a",
                  "v": "\v"}
        if e in simple:
            out.append(simple[e])
        elif e == "x":
            h = ""
            while i < n and body[i] in "0123456789abcdefABCDEF":
                h += body[i]
                i += 1
            out.append(chr(int(h, 16)) if h else "")
        elif e in ("u", "U"):
            width = 4 if e == "u" else 8
            h = body[i:i + width]
            i += width
            out.append(chr(int(h, 16)))
        else:
            out.append(e)
    return "".join(out)


def read_adjacent_literals(src, pos):
    """At src[pos] (skipping ws) tries to read one or more adjacent C++ string
    literals. Returns (decoded_value, end_index) or (None, pos) if not a
    string literal (e.g. a variable/expression argument)."""
    n = len(src)
    i = pos
    parts = []
    saw = False
    while i < n:
        while i < n and src[i] in " \t\r\n":
            i += 1
        # optional u8 prefix
        start = i
        if src.startswith("u8", i):
            i += 2
        if i < n and src[i] == '"':
            end = _end_of_string(src, i)
            parts.append(decode_cpp_string(src[start:end]))
            i = end
            saw = True
            continue
        break
    if not saw:
        return None, pos
    return "".join(parts), i


def visible_key(value):
    """Strips an ImGui '##id'/'###id' suffix; returns the visible prefix, or
    None if the label is a pure id / has no translatable letters."""
    idx = value.find("##")
    visible = value if idx < 0 else value[:idx]
    if not visible:
        return None
    # Remove printf specifiers first so "%s"/"%d" don't count their conversion
    # letter as translatable text; then require a real alphabetic character.
    without_fmt = re.sub(
        r"%[-+ 0#]*[0-9*]*(?:\.[0-9*]+)?(?:hh|h|ll|l|L|z|j|t)?"
        r"[diouxXeEfFgGaAcspn%]", "", visible)
    if not re.search(r"[A-Za-z]", without_fmt):
        return None  # pure format/punctuation like "%d", "->", "..."
    return visible


def format_specifiers(text):
    """Ordered list of printf conversion specifiers in text (ignores %%)."""
    specs = []
    i, n = 0, len(text)
    while i < n:
        if text[i] == "%":
            m = re.match(r"%[-+ 0#]*[0-9*]*(?:\.[0-9*]+)?(?:hh|h|ll|l|L|z|j|t)?"
                         r"([diouxXeEfFgGaAcspn%])", text[i:])
            if m:
                if m.group(1) != "%":
                    specs.append(m.group(1))
                i += m.end()
                continue
        i += 1
    return specs
