#!/usr/bin/env python3
"""
audit_panel_policies.py -- panel admission source audit for YAZE.

Every WindowContent subclass under src/app/editor must declare how users meet
it by overriding GetPresentationPolicy() (itself or through a base class), or
be registered with an explicit policy argument:

    window_manager->RegisterWindowContent(std::make_unique<Foo>(),
        WindowPresentationPolicy::EmbeddedTool().WithHost("Foo host"));

The runtime audit (test/unit/editor/panel_admission_test.cc) checks the
windows a booted EditorManager registers. It cannot see panels whose editors
need a ROM to load, or that are compiled out of the test binary, so this
script checks the source tree for the same rule with no build and no ROM.

Exit codes:
  0  -- every WindowContent subclass declares a policy
  1  -- classes that declare nothing were found
  2  -- usage error

Usage:
  python3 scripts/audit_panel_policies.py [--verbose] [--json]
  python3 scripts/audit_panel_policies.py --self-test

See "Panel admission" in docs/internal/architecture/editor-ui-module-pattern.md.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
SCAN_ROOT = PROJECT_ROOT / "src" / "app" / "editor"

# Root types a panel class derives from. `EditorPanel` is the legacy alias.
CONTENT_ROOTS = {"WindowContent", "EditorPanel"}
# Bases that already declare a policy for their subclasses.
DECLARING_ROOTS: set[str] = set()

POLICY_METHOD = "GetPresentationPolicy"

# Where FirstClassPanelAllowlist() and the deferred declarations live.
ADMISSION_SOURCE = "src/app/editor/system/workspace/panel_admission.cc"

# Sources that are not compiled (stale duplicates of compiled panels). They
# still define WindowContent classes, but nothing registers or links them.
# Delete the directory instead of growing this list.
IGNORED_PATH_PREFIXES = (
    # Duplicate of src/app/editor/oracle/panels/; not in editor_library.cmake.
    "src/app/editor/hack/oracle/ui/",
)

FIX_TEXT = (
    "Override GetPresentationPolicy() in the class and return one of:\n"
    "      WindowPresentationPolicy::EmbeddedTool().WithHost(\"<surface that "
    "embeds it>\")  // tool acting on another surface's selection\n"
    "      WindowPresentationPolicy::OptionalPopOut()  // standalone window "
    "users choose to open\n"
    "      WindowPresentationPolicy::Diagnostic().WithHost(\"Command "
    "palette\")  // developer/validation tooling\n"
    "      WindowPresentationPolicy::CoreWorkspace()  // an editor's primary "
    "surface; needs a FirstClassPanelAllowlist() entry in\n"
    "        src/app/editor/system/workspace/panel_admission.cc\n"
    "    See \"Panel admission\" in "
    "docs/internal/architecture/editor-ui-module-pattern.md."
)


@dataclass
class ClassInfo:
    name: str
    bases: list[str]
    path: str
    line: int
    body: str
    body_with_strings: str
    declares_policy: bool
    abstract: bool


@dataclass
class Finding:
    name: str
    path: str
    line: int

    def render(self) -> str:
        return (
            f"{self.path}:{self.line}: class {self.name} derives from "
            f"WindowContent but declares no presentation policy.\n"
            f"    Fix: {FIX_TEXT}"
        )


@dataclass
class Report:
    findings: list[Finding] = field(default_factory=list)
    scanned_classes: int = 0
    content_classes: int = 0
    registered_classes: int = 0
    explicit_at_registration: int = 0
    deferred: int = 0


def blank_comments_and_strings(text: str, keep_strings: bool = False) -> str:
    """Replace comments (and string/char literals unless keep_strings) with spaces.

    Newlines are kept so line numbers stay valid.
    """
    out = []
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if c == "/" and nxt == "/":
            while i < n and text[i] != "\n":
                out.append(" ")
                i += 1
        elif c == "/" and nxt == "*":
            out.append("  ")
            i += 2
            while i < n and not (text[i] == "*" and i + 1 < n and text[i + 1] == "/"):
                out.append("\n" if text[i] == "\n" else " ")
                i += 1
            out.append("  ")
            i += 2
        elif c == '"' or c == "'":
            quote = c
            out.append(quote)
            i += 1
            while i < n and text[i] != quote:
                if text[i] == "\\" and i + 1 < n:
                    out.append(text[i : i + 2] if keep_strings else "  ")
                    i += 2
                    continue
                if keep_strings:
                    out.append(text[i])
                else:
                    out.append("\n" if text[i] == "\n" else " ")
                i += 1
            out.append(quote)
            i += 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


CLASS_RE = re.compile(
    r"\b(?:class|struct)\s+(?:[A-Z_][A-Z0-9_]*\s+)*(\w+)\s*(?:final\s*)?:\s*([^{};]+)\{"
)


# A pure virtual member: `virtual void Draw() = 0;` (not `int x_ = 0;`).
PURE_VIRTUAL_RE = re.compile(r"\bvirtual\b[^;{}]*\)\s*(?:const)?\s*(?:noexcept)?\s*=\s*0\s*;")


def match_brace(text: str, open_index: int) -> int:
    depth = 0
    for i in range(open_index, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return i
    return len(text) - 1


def parse_bases(base_clause: str) -> list[str]:
    bases = []
    depth = 0
    current = []
    for ch in base_clause:
        if ch == "<":
            depth += 1
        elif ch == ">":
            depth -= 1
        if ch == "," and depth == 0:
            bases.append("".join(current))
            current = []
        else:
            current.append(ch)
    bases.append("".join(current))
    names = []
    for base in bases:
        base = re.sub(r"<.*>", "", base)
        tokens = [t for t in re.split(r"[\s:]+", base) if t]
        tokens = [t for t in tokens if t not in ("public", "protected", "private", "virtual")]
        if tokens:
            names.append(tokens[-1])
    return names


def parse_classes(path: str, raw_text: str) -> list[ClassInfo]:
    text = blank_comments_and_strings(raw_text)
    text_with_strings = blank_comments_and_strings(raw_text, keep_strings=True)
    classes = []
    for match in CLASS_RE.finditer(text):
        name = match.group(1)
        bases = parse_bases(match.group(2))
        open_index = match.end() - 1
        close_index = match_brace(text, open_index)
        body = text[open_index : close_index + 1]
        line = text.count("\n", 0, match.start()) + 1
        classes.append(
            ClassInfo(
                name=name,
                bases=bases,
                path=path,
                line=line,
                body=body,
                body_with_strings=text_with_strings[open_index : close_index + 1],
                declares_policy=POLICY_METHOD in body,
                abstract=PURE_VIRTUAL_RE.search(body) is not None,
            )
        )
    return classes


def split_top_level_args(arg_text: str) -> list[str]:
    args = []
    depth = 0
    current = []
    for ch in arg_text:
        if ch in "([{<":
            depth += 1
        elif ch in ")]}>":
            depth -= 1
        if ch == "," and depth == 0:
            args.append("".join(current).strip())
            current = []
        else:
            current.append(ch)
    tail = "".join(current).strip()
    if tail:
        args.append(tail)
    return args


def find_calls(text: str, function_name: str) -> list[tuple[int, list[str]]]:
    """Return (offset, top-level args) for each `function_name(...)` call."""
    calls = []
    for match in re.finditer(r"\b" + re.escape(function_name) + r"\s*\(", text):
        depth = 0
        start = match.end() - 1
        end = None
        for i in range(start, len(text)):
            if text[i] == "(":
                depth += 1
            elif text[i] == ")":
                depth -= 1
                if depth == 0:
                    end = i
                    break
        if end is None:
            continue
        calls.append((match.start(), split_top_level_args(text[start + 1 : end])))
    return calls


def registered_classes(raw_text: str) -> tuple[set[str], set[str]]:
    """Panel classes this file registers, and those it registers with a policy.

    Returns (registered, explicit). A class counts as registered when it is
    named in REGISTER_PANEL/REGISTER_PANEL_FACTORY or passed to
    RegisterWindowContent (directly through std::make_unique<T>, or through a
    local unique_ptr that was made with std::make_unique<T>). `explicit` holds
    the classes whose registration also passes a policy argument.
    """
    text = blank_comments_and_strings(raw_text)
    registered: set[str] = set()
    explicit: set[str] = set()

    for macro in ("REGISTER_PANEL", "REGISTER_WINDOW_CONTENT"):
        for match in re.finditer(r"\b" + macro + r"\s*\(\s*(\w+)\s*\)", text):
            registered.add(match.group(1))
    for macro in ("REGISTER_PANEL_FACTORY", "REGISTER_WINDOW_CONTENT_FACTORY"):
        for match in re.finditer(r"\b" + macro + r"\s*\(\s*(\w+)\s*,", text):
            registered.add(match.group(1))

    for _, args in find_calls(text, "RegisterWindowContent"):
        if not args:
            continue
        first = args[0]
        # The only two-argument overload takes a WindowPresentationPolicy, and
        # callers often wrap it in a helper, so any second argument counts.
        has_policy = len(args) >= 2
        name = None
        made = re.search(r"make_unique<\s*([\w:]+)\s*>", first)
        if made:
            name = made.group(1).split("::")[-1]
        else:
            moved = re.search(r"std::move\(\s*(\w+)\s*\)", first)
            if moved:
                var = moved.group(1)
                decl = re.search(
                    r"(?:auto|std::unique_ptr<\s*[\w:]+\s*>)\s+"
                    + re.escape(var)
                    + r"\s*(?:=|\{|\()\s*std::make_unique<\s*([\w:]+)\s*>",
                    text,
                )
                if decl:
                    name = decl.group(1).split("::")[-1]
                else:
                    # `auto panel = CreateFooPanel();` -- match Foo by snake case.
                    factory = re.search(
                        r"\bauto\s+" + re.escape(var) + r"\s*=\s*[\w:]+\s*\(", text
                    )
                    if factory:
                        name = "snake:" + var
        if name is None:
            continue
        registered.add(name)
        if has_policy:
            explicit.add(name)
    return registered, explicit


def camel_to_snake(name: str) -> str:
    return re.sub(r"(?<!^)(?=[A-Z])", "_", name).lower()


CONSTANT_RE = re.compile(
    r"\b(k\w+)\s*(?:\[\s*\])?\s*=\s*\"([^\"\n]+)\"\s*;"
)
DESCRIPTOR_ID_RE = re.compile(r"\.(?:card_id|id)\s*=\s*([^,}\n]+)")


def collect_constants(texts: dict[str, str]) -> dict[str, str]:
    """String constants such as `kRoomSelectorId = "dungeon.room_selector"`."""
    constants: dict[str, str] = {}
    for text in texts.values():
        for match in CONSTANT_RE.finditer(text):
            constants[match.group(1)] = match.group(2)
    return constants


def resolve_id(expression: str, constants: dict[str, str]) -> str | None:
    expression = expression.strip()
    literal = re.fullmatch(r'"([^"\n]*)"', expression)
    if literal:
        return literal.group(1)
    name = re.search(r"(k\w+)\s*$", expression)
    if name:
        return constants.get(name.group(1))
    return None


def enclosing_brace_group(text: str, index: int) -> tuple[int, int] | None:
    depth = 0
    for i in range(index, -1, -1):
        if text[i] == "}":
            depth += 1
        elif text[i] == "{":
            if depth == 0:
                return i, match_brace(text, i)
            depth -= 1
    return None


def declared_descriptor_ids(texts: dict[str, str], constants: dict[str, str]) -> set[str]:
    """Window ids whose registered descriptor carries an explicit `.presentation`.

    An editor may register a descriptor (icon, shortcut, priority) before it
    registers the content; the descriptor's declared policy then wins at
    runtime, so the content class does not need to repeat it.
    """
    ids: set[str] = set()
    for text in texts.values():
        for match in DESCRIPTOR_ID_RE.finditer(text):
            group = enclosing_brace_group(text, match.start())
            if group is None:
                continue
            body = text[group[0] : group[1] + 1]
            if ".presentation" not in body:
                continue
            resolved = resolve_id(match.group(1), constants)
            if resolved:
                ids.add(resolved)
    return ids


def class_window_id(info: "ClassInfo", texts: dict[str, str], constants: dict[str, str]) -> str | None:
    """The literal window id a class returns from GetId(), when statically known."""
    pattern = re.compile(r"\bGetId\s*\(\s*\)\s*const\s*(?:override\s*)?\{\s*return\s+([^;]+);")
    match = pattern.search(info.body_with_strings)
    if match:
        return resolve_id(match.group(1), constants)
    out_of_line = re.compile(
        r"\b" + re.escape(info.name) + r"::GetId\s*\(\s*\)\s*const\s*\{\s*return\s+([^;]+);"
    )
    for text in texts.values():
        found = out_of_line.search(text)
        if found:
            return resolve_id(found.group(1), constants)
    return None


DEFERRED_BLOCK_RE = re.compile(
    r"panel-admission: deferred-begin(.*?)panel-admission: deferred-end", re.S
)


def deferred_window_ids(sources: dict[str, str]) -> set[str]:
    """Window ids whose policy declaration is deferred (see panel_admission.cc)."""
    text = sources.get(ADMISSION_SOURCE, "")
    ids: set[str] = set()
    for block in DEFERRED_BLOCK_RE.findall(text):
        ids.update(re.findall(r'\{"([\w.]+)"\s*,', block))
    return ids


def audit_sources(sources: dict[str, str]) -> Report:
    """Audit `sources` (relative path -> text) and return findings."""
    report = Report()
    deferred_ids = deferred_window_ids(sources)
    sources = {
        path: text
        for path, text in sources.items()
        if not path.startswith(IGNORED_PATH_PREFIXES)
    }
    by_name: dict[str, list[ClassInfo]] = {}
    registered: set[str] = set()
    explicit: set[str] = set()
    texts_with_strings = {
        path: blank_comments_and_strings(text, keep_strings=True)
        for path, text in sources.items()
    }
    constants = collect_constants(texts_with_strings)
    declared_ids = declared_descriptor_ids(texts_with_strings, constants)
    for path, text in sources.items():
        for info in parse_classes(path, text):
            by_name.setdefault(info.name, []).append(info)
            report.scanned_classes += 1
        found_registered, found_explicit = registered_classes(text)
        registered |= found_registered
        explicit |= found_explicit

    # `snake:<var>` entries come from factory-made locals; resolve them by name.
    for entry in [e for e in registered if e.startswith("snake:")]:
        registered.discard(entry)
        var = entry[len("snake:") :]
        for name in by_name:
            if camel_to_snake(name) == var:
                registered.add(name)
                if entry in explicit:
                    explicit.add(name)
    explicit = {e for e in explicit if not e.startswith("snake:")}

    def is_content(name: str, seen: frozenset[str] = frozenset()) -> bool:
        if name in CONTENT_ROOTS:
            return True
        if name in seen:
            return False
        for info in by_name.get(name, []):
            if any(is_content(base, seen | {name}) for base in info.bases):
                return True
        return False

    def covered(info: ClassInfo, seen: frozenset[str] = frozenset()) -> bool:
        if info.declares_policy:
            return True
        if info.name in seen:
            return False
        for base in info.bases:
            if base in DECLARING_ROOTS:
                return True
            for parent in by_name.get(base, []):
                if covered(parent, seen | {info.name}):
                    return True
        return False

    for infos in by_name.values():
        for info in infos:
            if not any(is_content(base) for base in info.bases):
                continue
            report.content_classes += 1
            if info.abstract or info.name not in registered:
                continue
            report.registered_classes += 1
            if covered(info):
                continue
            if info.name in explicit:
                report.explicit_at_registration += 1
                continue
            window_id = class_window_id(info, texts_with_strings, constants)
            if window_id is not None and window_id in declared_ids:
                report.explicit_at_registration += 1
                continue
            if window_id is not None and window_id in deferred_ids:
                report.deferred += 1
                continue
            report.findings.append(Finding(info.name, info.path, info.line))
    report.findings.sort(key=lambda f: (f.path, f.line))
    return report


def load_tree() -> dict[str, str]:
    sources = {}
    for path in sorted(SCAN_ROOT.rglob("*")):
        if path.suffix not in (".h", ".cc"):
            continue
        relative = path.relative_to(PROJECT_ROOT).as_posix()
        sources[relative] = path.read_text(encoding="utf-8", errors="replace")
    return sources


def run_self_test() -> int:
    failures = []

    def check(label: str, condition: bool) -> None:
        if not condition:
            failures.append(label)

    declared = """
    class Declared : public WindowContent {
     public:
      WindowPresentationPolicy GetPresentationPolicy() const override {
        return WindowPresentationPolicy::OptionalPopOut();
      }
    };
    REGISTER_PANEL(Declared);
    """
    check("declared is clean", not audit_sources({"a.h": declared}).findings)

    undeclared = (
        "class Bare final : public WindowContent {"
        " public: void Draw(bool*) override {} };\n"
        "REGISTER_PANEL(Bare);"
    )
    report = audit_sources({"b.h": undeclared})
    check("undeclared registered class is reported", [f.name for f in report.findings] == ["Bare"])
    if report.findings:
        check("message names the class", "Bare" in report.findings[0].render())
        check("message tells the author what to do", "WithHost" in report.findings[0].render())

    unregistered = "class Helper : public WindowContent {};"
    check("classes never registered are ignored", not audit_sources({"b2.h": unregistered}).findings)

    inherited = declared + "\nclass Child : public Declared {};\nREGISTER_PANEL(Child);"
    check("inherited policy covers subclass", not audit_sources({"c.h": inherited}).findings)

    via_alias = "class Legacy : public EditorPanel {};\nREGISTER_PANEL(Legacy);"
    check("EditorPanel alias is content", len(audit_sources({"d.h": via_alias}).findings) == 1)

    transitive = (
        "class Mid : public WindowContent { int count_ = 0; virtual void Hook() = 0; };\n"
        "class Leaf : public Mid { int slot_ = 0; };\n"
        "REGISTER_PANEL(Mid);\nREGISTER_PANEL(Leaf);"
    )
    names = [f.name for f in audit_sources({"e.h": transitive}).findings]
    check("abstract base exempt, concrete leaf reported", names == ["Leaf"])

    not_content = "class Plain : public Other {};\nREGISTER_PANEL(Plain);"
    check("non-panel classes ignored", not audit_sources({"f.h": not_content}).findings)

    commented = "// class Ghost : public WindowContent {};\n// REGISTER_PANEL(Ghost);"
    check("comments ignored", not audit_sources({"g.h": commented}).findings)

    explicit_direct = (
        "class Tool : public WindowContent {};\n"
        "void Register(WorkspaceWindowManager* wm) {\n"
        "  wm->RegisterWindowContent(std::make_unique<Tool>(),\n"
        "      WindowPresentationPolicy::EmbeddedTool().WithHost(\"x\"));\n"
        "}\n"
    )
    check("explicit make_unique policy accepted", not audit_sources({"h.cc": explicit_direct}).findings)

    explicit_moved = (
        "class Tool2 : public WindowContent {};\n"
        "void Register(WorkspaceWindowManager* wm) {\n"
        "  auto tool = std::make_unique<Tool2>(1, 2);\n"
        "  wm->RegisterWindowContent(std::move(tool),\n"
        "                            WindowPresentationPolicy::Diagnostic());\n"
        "}\n"
    )
    check("explicit moved policy accepted", not audit_sources({"i.cc": explicit_moved}).findings)

    factory_made = (
        "class ToolPanel : public WindowContent {};\n"
        "void Register(WorkspaceWindowManager* wm) {\n"
        "  auto tool_panel = CreateToolPanel();\n"
        "  wm->RegisterWindowContent(std::move(tool_panel),\n"
        "                            WindowPresentationPolicy::Diagnostic());\n"
        "}\n"
    )
    check("factory-made local resolved by name", not audit_sources({"i2.cc": factory_made}).findings)

    no_arg = (
        "class Tool3 : public WindowContent {};\n"
        "void Register(WorkspaceWindowManager* wm) {\n"
        "  wm->RegisterWindowContent(std::make_unique<Tool3>());\n"
        "}\n"
    )
    check("registration without policy is reported", len(audit_sources({"j.cc": no_arg}).findings) == 1)

    resource = "class Room : public ResourceWindowContent {};\nREGISTER_PANEL(Room);"
    base = (
        "class ResourceWindowContent : public WindowContent {\n"
        " public:\n"
        "  WindowPresentationPolicy GetPresentationPolicy() const override {\n"
        "    return WindowPresentationPolicy::OptionalPopOut();\n"
        "  }\n"
        "};\n"
    )
    check(
        "resource base policy covers resource panels",
        not audit_sources({"k.h": base, "l.h": resource}).findings,
    )

    descriptor_declared = (
        "constexpr char kBenchId[] = \"test.bench\";\n"
        "class Bench : public WindowContent {\n"
        " public:\n"
        "  std::string GetId() const override { return kBenchId; }\n"
        "};\n"
        "void Register(WorkspaceWindowManager* wm) {\n"
        "  wm->RegisterPanel({.card_id = kBenchId,\n"
        "                     .presentation = WindowPresentationPolicy::CoreWorkspace()});\n"
        "  wm->RegisterWindowContent(std::make_unique<Bench>());\n"
        "}\n"
    )
    check(
        "descriptor that declared a policy covers its content",
        not audit_sources({"m.cc": descriptor_declared}).findings,
    )

    descriptor_undeclared = descriptor_declared.replace(
        ",\n                     .presentation = WindowPresentationPolicy::CoreWorkspace()", ""
    )
    check(
        "descriptor without a policy does not cover its content",
        len(audit_sources({"n.cc": descriptor_undeclared}).findings) == 1,
    )

    deferred_source = (
        "// panel-admission: deferred-begin\n"
        'const X k = {{"test.deferred", "reason"}};\n'
        "// panel-admission: deferred-end\n"
    )
    deferred_class = (
        "class Frozen : public WindowContent {\n"
        ' public: std::string GetId() const override { return "test.deferred"; }\n'
        "};\nREGISTER_PANEL(Frozen);"
    )
    check(
        "deferred window ids are exempt",
        not audit_sources({ADMISSION_SOURCE: deferred_source, "o.h": deferred_class}).findings,
    )
    check(
        "the same class without a deferral is reported",
        len(audit_sources({"o.h": deferred_class}).findings) == 1,
    )
    stale = {"src/app/editor/hack/oracle/ui/x.h": "class Old : public WindowContent {};\nREGISTER_PANEL(Old);"}
    check("stale duplicate directories are ignored", not audit_sources(stale).findings)

    if failures:
        for label in failures:
            print(f"SELF-TEST FAIL: {label}", file=sys.stderr)
        return 1
    print("audit_panel_policies self-test passed")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[1])
    parser.add_argument("--verbose", action="store_true")
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()

    if args.self_test:
        return run_self_test()

    report = audit_sources(load_tree())
    if args.json:
        print(
            json.dumps(
                {
                    "scanned_classes": report.scanned_classes,
                    "content_classes": report.content_classes,
                    "registered_classes": report.registered_classes,
                    "explicit_at_registration": report.explicit_at_registration,
                    "deferred": report.deferred,
                    "findings": [f.__dict__ for f in report.findings],
                },
                indent=2,
            )
        )
    else:
        if args.verbose or report.findings:
            print(
                f"scanned {report.scanned_classes} classes, "
                f"{report.content_classes} derive from WindowContent, "
                f"{report.registered_classes} are registered as windows "
                f"({report.explicit_at_registration} declare at registration, "
                f"{report.deferred} deferred)"
            )
        for finding in report.findings:
            print(finding.render())
        if report.findings:
            print(
                f"\n{len(report.findings)} panel class(es) declare no presentation "
                "policy. Registering a panel does not make it a first-class window; "
                "choose where users meet it."
            )
        elif args.verbose:
            print("every WindowContent subclass declares a presentation policy")
    return 1 if report.findings else 0


if __name__ == "__main__":
    sys.exit(main())
