#!/usr/bin/env python3
"""Static checks for the yaze_editor -> CLI dependency boundary.

yaze_editor links yaze_cli_editor_shared instead of the full yaze_cli_core, and
mac-editor builds with the agent stack stubbed out. No CI job builds that
configuration, so a new editor include of a CLI source would only fail at link
time there. These checks parse sources and CMake text; they never configure.
"""

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "src"
CLI_CORE_CMAKE = SRC / "cli/cli_core.cmake"
EDITOR_CMAKE = SRC / "app/editor/editor_library.cmake"

# Editor includes of agent-library headers whose symbols are referenced only
# under Z3ED_AI, or only for enum types.
GUARDED_AGENT_INCLUDES = {
    ("app/editor/system/workspace/proposal_drawer.h",
     "cli/service/planning/proposal_registry.h"),
}

LEAN_OFF = ("YAZE_BUILD_CLI", "YAZE_BUILD_Z3ED", "YAZE_BUILD_TESTS",
            "YAZE_ENABLE_AI", "YAZE_ENABLE_AI_RUNTIME", "YAZE_ENABLE_GRPC",
            "YAZE_ENABLE_REMOTE_AUTOMATION", "YAZE_BUILD_AGENT_UI",
            "YAZE_ENABLE_AGENT_CLI", "YAZE_ENABLE_HTTP_API")
FULL_ON = {
    "mac-ai": ("YAZE_ENABLE_AI", "YAZE_ENABLE_AI_RUNTIME", "YAZE_ENABLE_GRPC",
               "YAZE_ENABLE_REMOTE_AUTOMATION", "YAZE_BUILD_AGENT_UI",
               "YAZE_ENABLE_AGENT_CLI", "YAZE_ENABLE_HTTP_API",
               "YAZE_BUILD_TESTS"),
    "release": ("YAZE_ENABLE_AI", "YAZE_ENABLE_GRPC",
                "YAZE_ENABLE_REMOTE_AUTOMATION"),
}

INCLUDE = re.compile(r'^\s*#\s*include\s+"(cli/[^"]+\.h)"', re.M)
errors = []


def set_block(text, variable):
    match = re.search(r"set\(\s*%s\b(.*?)\n\)" % variable, text, re.S)
    if not match:
        raise SystemExit(f"{variable} not found")
    return {line.strip() for line in match.group(1).splitlines()
            if line.strip().endswith(".cc")}


def cli_includes(path):
    return set(INCLUDE.findall(path.read_text(encoding="utf-8")))


def compiled_source(header):
    source = header[:-2] + ".cc"
    return source if (SRC / source).exists() else None


cli_core_text = CLI_CORE_CMAKE.read_text(encoding="utf-8")
shared = set_block(cli_core_text, "YAZE_CLI_EDITOR_SHARED_SOURCES")
core = set_block(cli_core_text, "YAZE_CLI_CORE_SOURCES")
if shared & core:
    errors.append(f"sources listed in both CLI libraries: {sorted(shared & core)}")

# The shared library must not need yaze_cli_core objects.
pending = [Path(s) for s in shared] + [Path(s[:-3] + ".h") for s in shared]
seen = set()
while pending:
    relative = pending.pop()
    if relative in seen or not (SRC / relative).exists():
        continue
    seen.add(relative)
    for header in cli_includes(SRC / relative):
        pending.append(Path(header))
        source = compiled_source(header)
        if source and source not in shared:
            errors.append(f"{relative} includes {header}, but {source} is "
                          "not in YAZE_CLI_EDITOR_SHARED_SOURCES")

editor_root = SRC / "app/editor"
for path in sorted(editor_root.rglob("*")):
    if path.suffix not in (".cc", ".h") or "agent" in path.relative_to(editor_root).parts[:1]:
        continue
    relative = path.relative_to(SRC).as_posix()
    for header in sorted(cli_includes(path)):
        source = compiled_source(header)
        if (source and source not in shared
                and (relative, header) not in GUARDED_AGENT_INCLUDES):
            errors.append(f"{relative} includes {header}; add {source} to "
                          "YAZE_CLI_EDITOR_SHARED_SOURCES or guard the use")

editor_text = EDITOR_CMAKE.read_text(encoding="utf-8")
for block in re.findall(r"target_link_libraries\(\s*yaze_editor\b(.*?)\)", editor_text, re.S):
    if re.search(r"\byaze_cli_core\b", block):
        errors.append("yaze_editor links yaze_cli_core; link yaze_cli_editor_shared")
if not re.search(r"target_link_libraries\(\s*yaze_editor\s+PUBLIC[^)]*\byaze_cli_editor_shared\b",
                 editor_text):
    errors.append("yaze_editor no longer links yaze_cli_editor_shared")

presets = json.loads((ROOT / "CMakePresets.json").read_text(encoding="utf-8"))
configure = {preset["name"]: preset for preset in presets["configurePresets"]}


def cache(name):
    preset = configure[name]
    parents = preset.get("inherits", [])
    values = {}
    for parent in reversed([parents] if isinstance(parents, str) else parents):
        values.update(cache(parent))
    values.update(preset.get("cacheVariables", {}))
    return values


lean = cache("mac-editor")
for option in LEAN_OFF:
    if lean.get(option) != "OFF":
        errors.append(f"mac-editor must set {option}=OFF")
if lean.get("YAZE_BUILD_GUI") != "ON":
    errors.append("mac-editor must build the GUI")
for name, options in FULL_ON.items():
    values = cache(name)
    for option in options:
        if values.get(option) != "ON":
            errors.append(f"{name} must keep {option}=ON")
build_names = {preset["name"] for preset in presets["buildPresets"]}
if "mac-editor" not in build_names:
    errors.append("mac-editor build preset is missing")

if errors:
    print("\n".join(errors), file=sys.stderr)
    raise SystemExit(1)
print(f"Editor graph policy checks passed ({len(shared)} shared CLI sources).")
