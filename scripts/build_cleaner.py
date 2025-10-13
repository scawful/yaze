#!/usr/bin/env python3
"""Automate source list maintenance and self-header includes for YAZE."""

from __future__ import annotations

import argparse
from dataclasses import dataclass, field
import re
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Sequence, Set

PROJECT_ROOT = Path(__file__).resolve().parent.parent
SOURCE_ROOT = PROJECT_ROOT / "src"

SUPPORTED_EXTENSIONS = (".cc", ".c", ".cpp", ".cxx", ".mm")
HEADER_EXTENSIONS = (".h", ".hh", ".hpp", ".hxx")
BUILD_CLEANER_IGNORE_TOKEN = "build_cleaner:ignore"


@dataclass(frozen=True)
class DirectorySpec:
    path: Path
    recursive: bool = True
    extensions: Sequence[str] = SUPPORTED_EXTENSIONS

    def iter_files(self) -> Iterable[Path]:
        if not self.path.exists():
            return []
        if self.recursive:
            iterator = self.path.rglob("*")
        else:
            iterator = self.path.glob("*")
        for candidate in iterator:
            if candidate.is_file() and candidate.suffix in self.extensions:
                yield candidate


@dataclass
class CMakeSourceBlock:
    variable: str
    cmake_path: Path
    directories: Sequence[DirectorySpec]
    exclude: Set[Path] = field(default_factory=set)


CONFIG: Sequence[CMakeSourceBlock] = (
    CMakeSourceBlock(
        variable="YAZE_APP_EMU_SRC",
        cmake_path=SOURCE_ROOT / "CMakeLists.txt",
        directories=(DirectorySpec(SOURCE_ROOT / "app/emu"),),
    ),
    CMakeSourceBlock(
        variable="YAZE_APP_CORE_SRC",
        cmake_path=SOURCE_ROOT / "app/core/core_library.cmake",
        directories=(DirectorySpec(SOURCE_ROOT / "app/core", recursive=False),),
    ),
    CMakeSourceBlock(
        variable="YAZE_APP_GFX_SRC",
        cmake_path=SOURCE_ROOT / "app/gfx/gfx_library.cmake",
        directories=(DirectorySpec(SOURCE_ROOT / "app/gfx"),),
    ),
    CMakeSourceBlock(
        variable="YAZE_GUI_SRC",
        cmake_path=SOURCE_ROOT / "app/gui/gui_library.cmake",
        directories=(DirectorySpec(SOURCE_ROOT / "app/gui"),),
    ),
    CMakeSourceBlock(
        variable="YAZE_APP_EDITOR_SRC",
        cmake_path=SOURCE_ROOT / "app/editor/editor_library.cmake",
        directories=(DirectorySpec(SOURCE_ROOT / "app/editor"),),
    ),
    CMakeSourceBlock(
        variable="YAZE_APP_ZELDA3_SRC",
        cmake_path=SOURCE_ROOT / "zelda3/zelda3_library.cmake",
        directories=(DirectorySpec(SOURCE_ROOT / "zelda3"),),
    ),
    CMakeSourceBlock(
        variable="YAZE_NET_SRC",
        cmake_path=SOURCE_ROOT / "app/net/net_library.cmake",
        directories=(DirectorySpec(SOURCE_ROOT / "app/net"),),
        exclude={Path("app/net/rom_service_impl.cc")},
    ),
    CMakeSourceBlock(
        variable="YAZE_UTIL_SRC",
        cmake_path=SOURCE_ROOT / "util/util.cmake",
        directories=(DirectorySpec(SOURCE_ROOT / "util"),),
    ),
)


def relative_to_source(path: Path) -> Path:
    return path.relative_to(SOURCE_ROOT)


def parse_block(lines: List[str], start_idx: int) -> int:
    """Return index of the closing ')' line for a set/list block."""
    for idx in range(start_idx + 1, len(lines)):
        if lines[idx].strip().startswith(")"):
            return idx
    raise ValueError(f"Unterminated set/list block starting at line {start_idx}")


def parse_entry(line: str) -> Optional[str]:
    stripped = line.strip()
    if not stripped or stripped.startswith("#"):
        return None
    # Remove trailing inline comment
    stripped = stripped.split("#", 1)[0].strip()
    if not stripped:
        return None
    if stripped.startswith("$"):
        return None
    return stripped


def gather_expected_sources(block: CMakeSourceBlock) -> List[str]:
    entries: Set[str] = set()
    for directory in block.directories:
        for source_file in directory.iter_files():
            if should_ignore_path(source_file):
                continue
            rel_path = relative_to_source(source_file)
            if rel_path in block.exclude:
                continue
            entries.add(str(rel_path).replace("\\", "/"))
    return sorted(entries)


def should_ignore_path(path: Path) -> bool:
    try:
        with path.open("r", encoding="utf-8") as handle:
            head = handle.read(256)
    except (OSError, UnicodeDecodeError):
        return False
    return BUILD_CLEANER_IGNORE_TOKEN in head


def update_cmake_block(block: CMakeSourceBlock, dry_run: bool) -> bool:
    cmake_lines = (block.cmake_path.read_text(encoding="utf-8")).splitlines()
    pattern = re.compile(rf"\s*set\(\s*{re.escape(block.variable)}\b")

    start_idx: Optional[int] = None
    for idx, line in enumerate(cmake_lines):
        if pattern.match(line):
            start_idx = idx
            break

    if start_idx is None:
        for idx, line in enumerate(cmake_lines):
            stripped = line.strip()
            if not stripped.startswith("set("):
                continue
            remainder = stripped[4:].strip()
            if remainder:
                if remainder.startswith(block.variable):
                    start_idx = idx
                    break
                continue
            lookahead = idx + 1
            while lookahead < len(cmake_lines):
                next_line = cmake_lines[lookahead].strip()
                if not next_line or next_line.startswith("#"):
                    lookahead += 1
                    continue
                if next_line == block.variable:
                    start_idx = idx
                break
            if start_idx is not None:
                break

    if start_idx is None:
        raise ValueError(f"Could not locate set({block.variable}) in {block.cmake_path}")

    end_idx = parse_block(cmake_lines, start_idx)
    block_slice = cmake_lines[start_idx + 1 : end_idx]

    prelude: List[str] = []
    postlude: List[str] = []
    existing_entries: List[str] = []

    first_entry_idx: Optional[int] = None

    for idx, line in enumerate(block_slice):
        entry = parse_entry(line)
        if entry:
            if entry == block.variable and not existing_entries:
                prelude.append(line)
                continue
            existing_entries.append(entry)
            if first_entry_idx is None:
                first_entry_idx = idx
        else:
            if first_entry_idx is None:
                prelude.append(line)
            else:
                postlude.append(line)

    expected_entries = gather_expected_sources(block)
    expected_set = set(expected_entries)

    if set(existing_entries) == expected_set:
        return False

    indent = "  "
    if first_entry_idx is not None:
        sample_line = block_slice[first_entry_idx]
        indent = sample_line[: len(sample_line) - len(sample_line.lstrip())]

    rebuilt_block = prelude + [f"{indent}{entry}" for entry in expected_entries] + postlude

    if dry_run:
        print(f"[DRY-RUN] Would update {block.cmake_path.relative_to(PROJECT_ROOT)}")
        return True

    cmake_lines[start_idx + 1 : end_idx] = rebuilt_block
    block.cmake_path.write_text("\n".join(cmake_lines) + "\n", encoding="utf-8")
    print(f"Updated {block.cmake_path.relative_to(PROJECT_ROOT)}")
    missing = sorted(expected_set - set(existing_entries))
    removed = sorted(set(existing_entries) - expected_set)
    if missing:
        print(f"  Added: {', '.join(missing)}")
    if removed:
        print(f"  Removed: {', '.join(removed)}")
    return True


def find_self_header(source: Path) -> Optional[Path]:
    for ext in HEADER_EXTENSIONS:
        candidate = source.with_suffix(ext)
        if candidate.exists():
            return candidate
    return None


def has_include(lines: Sequence[str], header_variants: Iterable[str]) -> bool:
    include_patterns = {f'#include "{variant}"' for variant in header_variants}
    return any(line.strip() in include_patterns for line in lines)


def find_insert_index(lines: List[str]) -> int:
    include_block_start = None
    for idx, line in enumerate(lines):
        if line.startswith("#include"):
            include_block_start = idx
            break

    if include_block_start is not None:
        return include_block_start

    # No includes yet; skip leading comments/blank lines
    index = 0
    in_block_comment = False
    while index < len(lines):
        stripped = lines[index].strip()
        if not stripped:
            index += 1
            continue
        if stripped.startswith("/*") and not stripped.endswith("*/"):
            in_block_comment = True
            index += 1
            continue
        if in_block_comment:
            if "*/" in stripped:
                in_block_comment = False
            index += 1
            continue
        if stripped.startswith("//"):
            index += 1
            continue
        break
    return index


def ensure_self_header_include(source: Path, dry_run: bool) -> bool:
    if should_ignore_path(source):
        return False

    header = find_self_header(source)
    if header is None:
        return False

    try:
        lines = source.read_text(encoding="utf-8").splitlines()
    except UnicodeDecodeError:
        return False

    header_variants = {
        header.name,
        str(header.relative_to(SOURCE_ROOT)).replace("\\", "/"),
    }

    if has_include(lines, header_variants):
        return False

    include_line = f'#include "{header.name}"'

    insert_idx = find_insert_index(lines)
    lines.insert(insert_idx, include_line)

    if dry_run:
        rel = source.relative_to(PROJECT_ROOT)
        print(f"[DRY-RUN] Would insert self-header include into {rel}")
        return True

    source.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Inserted self-header include into {source.relative_to(PROJECT_ROOT)}")
    return True


def collect_source_files() -> Set[Path]:
    managed_dirs: Set[Path] = set()
    for block in CONFIG:
        for directory in block.directories:
            managed_dirs.add(directory.path)

    result: Set[Path] = set()
    for directory in managed_dirs:
        if not directory.exists():
            continue
        for file_path in directory.rglob("*"):
            if file_path.is_file() and file_path.suffix in SUPPORTED_EXTENSIONS:
                result.add(file_path)
    return result


def run(dry_run: bool, cmake_only: bool, includes_only: bool) -> int:
    if cmake_only and includes_only:
        raise ValueError("Cannot use --cmake-only and --includes-only together")

    changed = False

    if not includes_only:
        for block in CONFIG:
            changed |= update_cmake_block(block, dry_run)

    if not cmake_only:
        for source in collect_source_files():
            changed |= ensure_self_header_include(source, dry_run)

    if dry_run and not changed:
        print("No changes required (dry-run)")
    if not dry_run and not changed:
        print("No changes required")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Maintain CMake source lists and self-header includes.")
    parser.add_argument("--dry-run", action="store_true", help="Report prospective changes without editing files")
    parser.add_argument("--cmake-only", action="store_true", help="Only update CMake source lists")
    parser.add_argument("--includes-only", action="store_true", help="Only ensure self-header includes")
    args = parser.parse_args()

    try:
        return run(args.dry_run, args.cmake_only, args.includes_only)
    except Exception as exc:  # pylint: disable=broad-except
        print(f"build_cleaner failed: {exc}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
