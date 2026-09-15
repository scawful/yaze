#!/usr/bin/env python3
"""Generate and verify ALTTP reference tables against the usdasm disassembly.

Hand-written RAM and routine tables drift or get invented. This tool keeps
docs honest by resolving every address from usdasm instead:

  render --write FILE   Regenerate the <!-- BEGIN GENERATED: name --> sections.
  check FILE...         Fail when a `| NAME | $ADDR |` or `| $ADDR | NAME |`
                        table row disagrees with usdasm, or when generated
                        sections are stale.

usdasm is located via --usdasm, $YAZE_USDASM_DIR, ~/refs/usdasm (the pinned
checkout from scripts/cloud/bootstrap.sh refs), or ../usdasm.

Published usdasm commits do not ship WRAM/SRAM symbol maps. RAM symbols come
from --symbols DIR, $YAZE_USDASM_SYMBOLS_DIR, or ~/refs/jpdasm (spannerisms'
JP disassembly, which ships symbols_wram.asm/symbols_sram.asm). WRAM/SRAM
layout is shared by the JP and US ROMs for the curated rows; see
docs/internal/zelda3/alttp-quick-reference.md for the US usage audit. Without
symbol maps those rows are reported as unverified rather than failing.
"""

from __future__ import annotations

import argparse
import os
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

# Curated names from the jpdasm symbol maps. Addresses and descriptions are
# read from the maps; an unknown name is an error.
RAM_SYMBOLS = [
    "MODE", "SUBMODE", "INDOORS", "POSY", "POSX", "OWSCR", "ROOM",
    "LINKDO", "DUNGEON", "WORLDFLAG", "SONG", "LASTSONG",
    "SPR0_YL", "SPR0_XL", "SPR0_STATE", "SPR0_TIMER_A", "SPR0_ID",
    "SPR0_HP",
]
SRAM_SYMBOLS = [
    "BOW", "BOOMER", "HOOKSHOT", "BOMBS", "GLOVES", "BOOTS", "FLIPPERS",
    "PEARL", "SWORD", "SHIELD", "ARMOR", "RUPEES", "MAXHP", "CURHP",
    "MAGPOW", "KEYS", "PENDANTS", "CRYSTALS", "GAMESTATE",
]

# The symbol-map comments describe the JP disassembly and occasionally contain
# notes that are wrong for the US ROM documented by the quick reference. Keep
# corrections explicit and tested so regeneration cannot restore known errors.
RAM_NOTE_OVERRIDES = {
    "ROOM": ("Room ID for underworld; US code copies $A0 to $048E "
             "(not $0483); $A1 is expected to be 0 or 1"),
}

# Logical symbol source -> accepted filenames (usdasm-style, jpdasm-style).
SYMBOL_FILES = {
    "wram": ("wram.asm", "symbols_wram.asm"),
    "sram": ("sram.asm", "symbols_sram.asm"),
    "registers": ("registers.asm",),
}

# (label, purpose). Purposes stay generic on purpose: they restate what the
# label and its dispatch site establish, nothing more.
ROUTINES = [
    ("Reset", "Power-on entry (RESET vector)"),
    ("MainGameLoop", "Per-frame main loop"),
    ("RunModule", "Dispatches MODE through the module pointer tables"),
    ("NMI", "NMI (V-blank) handler"),
    ("IRQ", "IRQ handler"),
    ("ReadJoypad", "Joypad read during NMI"),
    ("ClearOAMBuffer", "Clears the OAM buffer"),
    ("Module05_LoadFile", "Module 0x05 entry"),
    ("Module06_UnderworldLoad", "Module 0x06 entry"),
    ("Module07_Underworld", "Module 0x07 entry"),
    ("Module09_Overworld", "Module 0x09 entry"),
    ("LoadAndBuildRoom", "Underworld room load"),
    ("RoomDraw_DrawAllObjects", "Draws a room's object streams"),
    ("Intro_LoadAllPalettes_long", "Full palette load (long entry)"),
    ("Link", "Link per-frame entry; dispatches the LinkState vectors"),
    ("SpawnSecret", "Spawns secrets from destroyed terrain"),
    ("Ancilla_AddHitStars", "Adds the hit-stars ancilla"),
    ("LoadUnderworldTileTypes", "Loads underworld tile types"),
    ("LoadDefaultTileTypes", "Loads default tile types"),
    ("Module1A_Credits", "Module 0x1A entry"),
]

MODULE_TABLE = (0x008061, 0x1C, 1)       # RunModule .low table
LINK_STATE_TABLE = (0x078041, 0x1F, 2)   # Link state vectors
VECTOR_RANGE = (0x00FFE4, 0x00FFFE)

SYM_RE = re.compile(r"^([A-Za-z0-9_]+)\s*=\s*\$([0-9A-Fa-f]{2,6})\b")
# usdasm prefixes alias/secondary labels with `#` (e.g. `#obj09B8:`).
LABEL_RE = re.compile(r"^#?([A-Za-z][A-Za-z0-9_]*):\s*(;.*)?$")
# Classic usdasm/jpdasm address column (`#_008034: LDA.b $12`). Upstream's
# Futaba format (`|008034| ...`) needs a new pattern before the pins move.
PC_RE = re.compile(r"^#_([0-9A-F]{6}):\s*(.*)$")
DATA_RE = re.compile(r"^(db|dw|dl)\s+([A-Za-z_][A-Za-z0-9_]*|\$[0-9A-Fa-f]+)")
ROW_NAME_ADDR = re.compile(
    r"^\|\s*`?([A-Za-z_][A-Za-z0-9_]*)`?\s*\|\s*`?\$([0-9A-Fa-f]{4,6})`?\s*\|")
ROW_ADDR_NAME = re.compile(
    r"^\|\s*\*{0,2}`?\$([0-9A-Fa-f]{4,6})`?\*{0,2}\s*\|\s*\*{0,2}`?"
    r"([A-Za-z_][A-Za-z0-9_]*)`?\*{0,2}\s*\|")
SECTION_RE = re.compile(
    r"<!-- BEGIN GENERATED: (\w+) -->\n.*?<!-- END GENERATED: \1 -->", re.S)


@dataclass
class Usdasm:
    root: Path
    symbols: dict[str, int] = field(default_factory=dict)
    symbol_notes: dict[str, str] = field(default_factory=dict)
    labels: dict[str, int] = field(default_factory=dict)
    data: dict[int, str] = field(default_factory=dict)
    sources: dict[str, Path] = field(default_factory=dict)

    @classmethod
    def load(cls, root: Path, symbols_root: Path | None = None) -> "Usdasm":
        db = cls(root)
        # Published usdasm commits ship registers.asm but no WRAM/SRAM maps;
        # those come from symbols_root when given.
        for source, filenames in SYMBOL_FILES.items():
            candidates = [base / name for base in (root, symbols_root)
                          if base is not None for name in filenames]
            path = next((c for c in candidates if c.is_file()), None)
            if path is not None:
                db._load_symbols(path)
                db.sources[source] = path
        for path in sorted(root.glob("bank_*.asm")):
            db._load_bank(path)
        return db

    @property
    def has_ram_symbols(self) -> bool:
        return "wram" in self.sources and "sram" in self.sources

    def _load_symbols(self, path: Path) -> None:
        # A contiguous comment block describes the symbols directly below it;
        # a blank or non-symbol line ends the block.
        comments: list[str] = []
        for line in path.read_text(errors="replace").splitlines():
            stripped = line.strip()
            if stripped.startswith(";"):
                text = stripped.lstrip(";").strip()
                if text and not set(text) <= set("-="):
                    comments.append(text)
                continue
            match = SYM_RE.match(stripped)
            if not match:
                comments = []
                continue
            name, addr = match.group(1), int(match.group(2), 16)
            previous = self.symbols.setdefault(name, addr)
            if previous != addr:
                raise SystemExit(f"{path}: symbol {name} redefined as {snes(addr)} "
                                 f"(already {snes(previous)})")
            if comments:
                self.symbol_notes.setdefault(name, "; ".join(comments))

    def _load_bank(self, path: Path) -> None:
        pending: list[str] = []
        for line in path.read_text(errors="replace").splitlines():
            label = LABEL_RE.match(line)
            if label:
                pending.append(label.group(1))
                continue
            pc = PC_RE.match(line)
            if pc:
                addr = int(pc.group(1), 16)
                for name in pending:
                    self.labels.setdefault(name, addr)
                pending = []
                data = DATA_RE.match(pc.group(2))
                if data:
                    self.data[addr] = data.group(2)
                continue
            stripped = line.strip()
            # Asar scope braces and local `.labels` sit between a routine
            # label and its first instruction.
            if stripped in ("{", "}") or stripped.startswith((";", ".")):
                continue
            if stripped:
                pending = []

    def resolve(self, name: str) -> int | None:
        if name in self.symbols:
            return self.symbols[name]
        return self.labels.get(name)


def _configured(explicit: str | None, flag: str,
                env_var: str) -> tuple[str, str] | None:
    """Return (value, origin) for an explicitly configured location, if any."""
    if explicit:
        return explicit, flag
    if os.environ.get(env_var):
        return os.environ[env_var], "$" + env_var
    return None


def find_usdasm(explicit: str | None) -> Path:
    configured = _configured(explicit, "--usdasm", "YAZE_USDASM_DIR")
    if configured:
        # An explicit location that is wrong is an error, not a fallback.
        value, origin = configured
        if not (Path(value) / "bank_00.asm").is_file():
            sys.exit(f"usdasm not found at {value} (from {origin}): no bank_00.asm")
        return Path(value)
    repo = Path(__file__).resolve().parents[2]
    for candidate in (Path.home() / "refs/usdasm", repo.parent / "usdasm"):
        if (candidate / "bank_00.asm").is_file():
            return candidate
    sys.exit("usdasm not found: pass --usdasm or set YAZE_USDASM_DIR")


def _has_symbol_maps(base: Path) -> bool:
    return any((base / name).is_file()
               for names in (SYMBOL_FILES["wram"], SYMBOL_FILES["sram"])
               for name in names)


def find_symbols(explicit: str | None) -> Path | None:
    configured = _configured(explicit, "--symbols", "YAZE_USDASM_SYMBOLS_DIR")
    if configured:
        value, origin = configured
        if not _has_symbol_maps(Path(value)):
            sys.exit(f"no WRAM/SRAM symbol maps at {value} (from {origin}): expected "
                     "symbols_wram.asm/symbols_sram.asm or wram.asm/sram.asm")
        return Path(value)
    default = Path.home() / "refs/jpdasm"
    return default if _has_symbol_maps(default) else None


def looks_like_ram(addr: int) -> bool:
    """True for WRAM/SRAM doc addresses, including 4-digit shorthand.

    Six-digit addresses must be in banks $7E/$7F. Four-digit shorthand is
    treated as RAM in the low WRAM mirror ($0000-$1FFF) and the $F000+ save
    area ($7EF000 written as $Fxxx); register and ROM shorthand falls outside.
    """
    if addr > 0xFFFF:
        return addr >> 16 in (0x7E, 0x7F)
    return addr < 0x2000 or addr >= 0xF000


def addresses_match(actual: int, doc: int) -> bool:
    """Six-digit doc addresses must match exactly.

    Four-digit doc addresses compare only the low 16 bits, so `$2100` matches
    `INIDISP = $002100` and `$80B5` matches a `$0080B5` label. The trade-off:
    shorthand cannot catch a wrong bank; write six digits when the bank matters.
    """
    if doc > 0xFFFF:
        return actual == doc
    return actual & 0xFFFF == doc


def snes(addr: int) -> str:
    return f"${addr:06X}"


def short(text: str, limit: int = 110) -> str:
    text = text.replace("|", "\\|")
    return text if len(text) <= limit else text[: limit - 1].rstrip() + "…"


def render_ram(db: Usdasm, names: list[str]) -> str:
    rows = ["| Symbol | Address | Reference note |", "|---|---|---|"]
    for name in names:
        addr = db.symbols.get(name)
        if addr is None:
            raise SystemExit(f"unknown usdasm symbol: {name}")
        note = RAM_NOTE_OVERRIDES.get(name, db.symbol_notes.get(name, ""))
        rows.append(f"| `{name}` | `{snes(addr)}` | "
                    f"{short(note)} |")
    return "\n".join(rows)


def render_dispatch(db: Usdasm, start: int, count: int, step: int) -> str:
    rows = ["| ID | Target |", "|---|---|"]
    for index in range(count):
        target = db.data.get(start + index * step)
        if target is None:
            raise SystemExit(f"no table entry at {snes(start + index * step)}")
        rows.append(f"| `0x{index:02X}` | `{target}` |")
    return "\n".join(rows)


def render_routines(db: Usdasm) -> str:
    rows = ["| Routine | Address | Purpose |", "|---|---|---|"]
    for name, purpose in ROUTINES:
        addr = db.labels.get(name)
        if addr is None:
            raise SystemExit(f"unknown usdasm label: {name}")
        rows.append(f"| `{name}` | `{snes(addr)}` | {purpose} |")
    return "\n".join(rows)


def render_vectors(db: Usdasm) -> str:
    # Group by handler so each row is `| handler | handler address | ... |`,
    # the same shape `check` validates.
    vectors: dict[str, list[int]] = {}
    for addr in range(VECTOR_RANGE[0], VECTOR_RANGE[1] + 1, 2):
        target = db.data.get(addr)
        if target is not None:
            vectors.setdefault(target, []).append(addr)
    rows = ["| Handler | Handler address | Vector slots |", "|---|---|---|"]
    for target, slots in vectors.items():
        where = ", ".join(f"`{snes(a)}`" for a in slots)
        if target.startswith("$"):
            rows.append(f"| (none: `{target}`) | — | {where} |")
        else:
            rows.append(f"| `{target}` | `{snes(db.labels[target])}` | {where} |")
    return "\n".join(rows)


def render_bank_families(db: Usdasm) -> str:
    families: dict[int, dict[str, int]] = {}
    for name, addr in db.labels.items():
        bank = addr >> 16
        prefix = name.split("_", 1)[0] if "_" in name else name
        families.setdefault(bank, {}).setdefault(prefix, 0)
        families[bank][prefix] += 1
    rows = ["| Bank | Labels | Most common label prefixes |", "|---|---|---|"]
    for bank in sorted(families):
        counts = families[bank]
        top = sorted(counts.items(), key=lambda kv: (-kv[1], kv[0]))[:4]
        summary = ", ".join(f"`{p}_` ({n})" for p, n in top)
        rows.append(f"| `${bank:02X}` | {sum(counts.values())} | {summary} |")
    return "\n".join(rows)


KNOWN_SECTIONS = ("modules", "link_states", "wram", "sram", "routines",
                  "vectors", "banks")


def generated_sections(db: Usdasm) -> dict[str, str]:
    sections = {
        "modules": render_dispatch(db, *MODULE_TABLE),
        "link_states": render_dispatch(db, *LINK_STATE_TABLE),
        "routines": render_routines(db),
        "vectors": render_vectors(db),
        "banks": render_bank_families(db),
    }
    if db.has_ram_symbols:
        sections["wram"] = render_ram(db, RAM_SYMBOLS)
        sections["sram"] = render_ram(db, SRAM_SYMBOLS)
    return sections


def apply_sections(text: str, sections: dict[str, str]) -> str:
    def replace(match: re.Match[str]) -> str:
        name = match.group(1)
        if name not in KNOWN_SECTIONS:
            raise SystemExit(f"unknown generated section: {name}")
        if name not in sections:
            return match.group(0)  # source files absent; leave as-is
        return (f"<!-- BEGIN GENERATED: {name} -->\n{sections[name]}\n"
                f"<!-- END GENERATED: {name} -->")
    return SECTION_RE.sub(replace, text)


def check_file(db: Usdasm, path: Path, sections: dict[str, str],
               unverified: list[str]) -> list[str]:
    problems = []
    text = path.read_text()
    for match in SECTION_RE.finditer(text):
        if match.group(1) in KNOWN_SECTIONS and match.group(1) not in sections:
            unverified.append(f"{path}: generated section {match.group(1)}")
    if SECTION_RE.search(text) and apply_sections(text, sections) != text:
        problems.append(f"{path}: generated sections are stale; run render --write")
    for number, line in enumerate(text.splitlines(), 1):
        match = ROW_NAME_ADDR.match(line)
        if match:
            name, addr = match.group(1), int(match.group(2), 16)
        else:
            match = ROW_ADDR_NAME.match(line)
            if not match:
                continue
            addr, name = int(match.group(1), 16), match.group(2)
        actual = db.resolve(name)
        if actual is None and not db.has_ram_symbols and looks_like_ram(addr):
            unverified.append(f"{path}:{number}")
            continue
        if actual is None:
            problems.append(f"{path}:{number}: `{name}` is not a usdasm symbol or label")
        elif not addresses_match(actual, addr):
            problems.append(f"{path}:{number}: `{name}` is {snes(actual)} in usdasm, "
                            f"doc says {snes(addr)}")
    return problems


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--usdasm")
    parser.add_argument("--symbols",
                        help="directory with WRAM/SRAM symbol maps (default: "
                             "$YAZE_USDASM_SYMBOLS_DIR or ~/refs/jpdasm)")
    sub = parser.add_subparsers(dest="command", required=True)
    render = sub.add_parser("render")
    render.add_argument("--write", type=Path, required=True)
    check = sub.add_parser("check")
    check.add_argument("files", type=Path, nargs="+")
    args = parser.parse_args(argv)

    db = Usdasm.load(find_usdasm(args.usdasm), find_symbols(args.symbols))
    sections = generated_sections(db)
    if args.command == "render":
        original = args.write.read_text()
        args.write.write_text(apply_sections(original, sections))
        return 0

    unverified: list[str] = []
    problems = [p for f in args.files
                for p in check_file(db, f, sections, unverified)]
    if unverified:
        print(f"note: no WRAM/SRAM symbol maps found; {len(unverified)} RAM "
              "row(s)/section(s) were not verified")
    for problem in problems:
        print(problem)
    print(f"checked {len(args.files)} file(s): {len(problems)} problem(s)")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
