#!/usr/bin/env python3
"""Automate source list maintenance and self-header includes for YAZE."""

from __future__ import annotations

import argparse
from dataclasses import dataclass, field
import re
from pathlib import Path
from typing import Any, Iterable, List, Optional, Sequence, Set

try:
    import pathspec
    HAS_PATHSPEC = True
except ImportError:
    HAS_PATHSPEC = False
    print("Warning: 'pathspec' module not found. Install with: pip3 install pathspec")
    print("         .gitignore support will be disabled.")

PROJECT_ROOT = Path(__file__).resolve().parent.parent
SOURCE_ROOT = PROJECT_ROOT / "src"

SUPPORTED_EXTENSIONS = (".cc", ".c", ".cpp", ".cxx", ".mm")
HEADER_EXTENSIONS = (".h", ".hh", ".hpp", ".hxx")
BUILD_CLEANER_IGNORE_TOKEN = "build_cleaner:ignore"

# Common SNES/ROM header patterns to include
COMMON_HEADERS = {
    'std::': ['<memory>', '<string>', '<vector>', '<map>', '<set>', '<algorithm>', '<functional>'],
    'absl::': ['<absl/status/status.h>', '<absl/status/statusor.h>', '<absl/strings/str_format.h>'],
    'ImGui::': ['<imgui.h>'],
    'SDL_': ['<SDL.h>'],
}


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


def load_gitignore():
    """Load .gitignore patterns into a pathspec matcher."""
    if not HAS_PATHSPEC:
        return None
        
    gitignore_path = PROJECT_ROOT / ".gitignore"
    if not gitignore_path.exists():
        return None
    
    try:
        with gitignore_path.open('r', encoding='utf-8') as f:
            patterns = [line.strip() for line in f if line.strip() and not line.startswith('#')]
        return pathspec.PathSpec.from_lines('gitwildmatch', patterns)
    except Exception as e:
        print(f"Warning: Could not load .gitignore: {e}")
        return None


def is_ignored(path: Path, gitignore_spec) -> bool:
    """Check if a path should be ignored based on .gitignore patterns."""
    if gitignore_spec is None or not HAS_PATHSPEC:
        return False
    
    try:
        rel_path = path.relative_to(PROJECT_ROOT)
        return gitignore_spec.match_file(str(rel_path))
    except ValueError:
        return False


def discover_cmake_libraries() -> List[CMakeSourceBlock]:
    """
    Auto-discover CMake library files that explicitly opt-in to auto-maintenance.
    
    Looks for marker comments like:
    - "# build_cleaner:auto-maintain"
    - "# auto-maintained by build_cleaner.py"
    - "# AUTO-MAINTAINED"
    
    Only source lists with these markers will be updated.
    
    Supports decomposed libraries where one cmake file defines multiple PREFIX_SUBDIR_SRC
    variables (e.g., GFX_TYPES_SRC, GFX_BACKEND_SRC). Automatically scans subdirectories.
    """
    # First pass: collect all variables per cmake file
    file_variables: dict[Path, list[str]] = {}
    
    for cmake_file in SOURCE_ROOT.rglob("*.cmake"):
        if 'lib/' in str(cmake_file) or 'third_party/' in str(cmake_file):
            continue
            
        try:
            content = cmake_file.read_text(encoding='utf-8')
            lines = content.splitlines()
            
            for i, line in enumerate(lines):
                # Check if previous lines indicate auto-maintenance
                auto_maintained = False
                for j in range(max(0, i-5), i):
                    line_lower = lines[j].lower()
                    if ('build_cleaner' in line_lower and 'auto-maintain' in line_lower) or \
                       'auto_maintain' in line_lower:
                        auto_maintained = True
                        break
                
                if not auto_maintained:
                    continue
                
                # Extract variable name (allow for line breaks or closing paren)
                match = re.search(r'set\s*\(\s*(\w+(?:_SRC|_SOURCES|_SOURCE))(?:\s|$|\))', line)
                if match:
                    var_name = match.group(1)
                    if cmake_file not in file_variables:
                        file_variables[cmake_file] = []
                    if var_name not in file_variables[cmake_file]:
                        file_variables[cmake_file].append(var_name)
                        
        except Exception as e:
            print(f"Warning: Could not process {cmake_file}: {e}")
    
    # Second pass: create blocks with proper subdirectory detection
    blocks = []
    for cmake_file, variables in file_variables.items():
        cmake_dir = cmake_file.parent
        is_recursive = cmake_dir != SOURCE_ROOT / "app/core"
        
        # Analyze variable naming patterns to detect decomposed libraries
        # Group variables by prefix (e.g., GFX_*, GUI_*, EDITOR_*)
        prefix_groups: dict[str, list[str]] = {}
        for var_name in variables:
            match = re.match(r'([A-Z]+)_([A-Z_]+)_(?:SRC|SOURCES|SOURCE)$', var_name)
            if match:
                prefix = match.group(1)
                if prefix not in prefix_groups:
                    prefix_groups[prefix] = []
                prefix_groups[prefix].append(var_name)
        
        # If a prefix has multiple variables, treat it as a decomposed library
        decomposed_prefixes = {p for p, vars in prefix_groups.items() if len(vars) >= 2}
        
        for var_name in variables:
            # Try to extract subdirectory from variable name
            subdir_match = re.match(r'([A-Z]+)_([A-Z_]+)_(?:SRC|SOURCES|SOURCE)$', var_name)
            if subdir_match:
                prefix = subdir_match.group(1)
                subdir_part = subdir_match.group(2)
                
                # If this prefix indicates a decomposed library, scan subdirectory
                if prefix in decomposed_prefixes:
                    subdir = subdir_part.lower()
                    target_dir = cmake_dir / subdir
                    
                    if target_dir.exists() and target_dir.is_dir():
                        blocks.append(CMakeSourceBlock(
                            variable=var_name,
                            cmake_path=cmake_file,
                            directories=(DirectorySpec(target_dir, recursive=True),),
                        ))
                        continue
            
            # Fallback: scan entire cmake directory
            blocks.append(CMakeSourceBlock(
                variable=var_name,
                cmake_path=cmake_file,
                directories=(DirectorySpec(cmake_dir, recursive=is_recursive),),
            ))
    
    return blocks


# Static configuration for all library source lists
# The script now auto-maintains all libraries while preserving conditional sections
STATIC_CONFIG: Sequence[CMakeSourceBlock] = (
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
    # Note: These are commented out in favor of auto-discovery via markers
    # CMakeSourceBlock(
    #     variable="GFX_TYPES_SRC",
    #     cmake_path=SOURCE_ROOT / "app/gfx/gfx_library.cmake",
    #     directories=(DirectorySpec(SOURCE_ROOT / "app/gfx/types"),),
    # ),
    # CMakeSourceBlock(
    #     variable="GFX_BACKEND_SRC",
    #     cmake_path=SOURCE_ROOT / "app/gfx/gfx_library.cmake",
    #     directories=(DirectorySpec(SOURCE_ROOT / "app/gfx/backend"),),
    # ),
    # CMakeSourceBlock(
    #     variable="GFX_RESOURCE_SRC",
    #     cmake_path=SOURCE_ROOT / "app/gfx/gfx_library.cmake",
    #     directories=(DirectorySpec(SOURCE_ROOT / "app/gfx/resource"),),
    # ),
    # CMakeSourceBlock(
    #     variable="GFX_CORE_SRC",
    #     cmake_path=SOURCE_ROOT / "app/gfx/gfx_library.cmake",
    #     directories=(DirectorySpec(SOURCE_ROOT / "app/gfx/core"),),
    # ),
    # CMakeSourceBlock(
    #     variable="GFX_UTIL_SRC",
    #     cmake_path=SOURCE_ROOT / "app/gfx/gfx_library.cmake",
    #     directories=(DirectorySpec(SOURCE_ROOT / "app/gfx/util"),),
    # ),
    # CMakeSourceBlock(
    #     variable="GFX_RENDER_SRC",
    #     cmake_path=SOURCE_ROOT / "app/gfx/gfx_library.cmake",
    #     directories=(DirectorySpec(SOURCE_ROOT / "app/gfx/render"),),
    # ),
    # CMakeSourceBlock(
    #     variable="GFX_DEBUG_SRC",
    #     cmake_path=SOURCE_ROOT / "app/gfx/gfx_library.cmake",
    #     directories=(DirectorySpec(SOURCE_ROOT / "app/gfx/debug"),),
    # ),
    CMakeSourceBlock(
        variable="GUI_CORE_SRC",
        cmake_path=SOURCE_ROOT / "app/gui/gui_library.cmake",
        directories=(DirectorySpec(SOURCE_ROOT / "app/gui/core"),),
    ),
    CMakeSourceBlock(
        variable="CANVAS_SRC",
        cmake_path=SOURCE_ROOT / "app/gui/gui_library.cmake",
        directories=(DirectorySpec(SOURCE_ROOT / "app/gui/canvas"),),
    ),
    CMakeSourceBlock(
        variable="GUI_WIDGETS_SRC",
        cmake_path=SOURCE_ROOT / "app/gui/gui_library.cmake",
        directories=(DirectorySpec(SOURCE_ROOT / "app/gui/widgets"),),
    ),
    CMakeSourceBlock(
        variable="GUI_AUTOMATION_SRC",
        cmake_path=SOURCE_ROOT / "app/gui/gui_library.cmake",
        directories=(DirectorySpec(SOURCE_ROOT / "app/gui/automation"),),
    ),
    CMakeSourceBlock(
        variable="GUI_APP_SRC",
        cmake_path=SOURCE_ROOT / "app/gui/gui_library.cmake",
        directories=(DirectorySpec(SOURCE_ROOT / "app/gui/app"),),
    ),
    CMakeSourceBlock(
        variable="YAZE_AGENT_SOURCES",
        cmake_path=SOURCE_ROOT / "cli/agent.cmake",
        directories=(
            DirectorySpec(SOURCE_ROOT / "cli", recursive=False),  # For flags.cc
            DirectorySpec(SOURCE_ROOT / "cli/service"),
            DirectorySpec(SOURCE_ROOT / "cli/handlers"),
        ),
        exclude={
            Path("cli/cli.cc"),  # Part of z3ed executable
            Path("cli/cli_main.cc"),  # Part of z3ed executable
        },
    ),
    CMakeSourceBlock(
        variable="YAZE_TEST_SOURCES",
        cmake_path=SOURCE_ROOT / "app/test/test.cmake",
        directories=(DirectorySpec(SOURCE_ROOT / "app/test"),),
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


def extract_conditional_files(cmake_path: Path, variable: str) -> Set[str]:
    """Extract files that are added to the variable via conditional blocks (if/endif)."""
    conditional_files: Set[str] = set()
    
    try:
        lines = cmake_path.read_text(encoding='utf-8').splitlines()
    except Exception:
        return conditional_files
    
    in_conditional = False
    conditional_depth = 0
    
    for i, line in enumerate(lines):
        stripped = line.strip()
        
        # Track if/endif blocks
        if stripped.startswith('if(') or stripped.startswith('if '):
            if in_conditional:
                conditional_depth += 1
            else:
                in_conditional = True
                conditional_depth = 0
        elif stripped.startswith('endif(') or stripped == 'endif()':
            if conditional_depth > 0:
                conditional_depth -= 1
            else:
                in_conditional = False
        
        # Check if this line appends to our variable
        if in_conditional and f'APPEND {variable}' in line:
            # Handle single-line list(APPEND VAR file.cc)
            if ')' in line:
                # Extract file from same line
                match = re.search(rf'APPEND\s+{re.escape(variable)}\s+(.+?)\)', line)
                if match:
                    file_str = match.group(1).strip()
                    # Can be multiple files separated by space
                    for f in file_str.split():
                        f = f.strip()
                        if f and not f.startswith('$') and '/' in f and f.endswith('.cc'):
                            conditional_files.add(f)
            else:
                # Multi-line list(APPEND) - extract files from following lines
                j = i + 1
                while j < len(lines) and not lines[j].strip().startswith(')'):
                    entry = parse_entry(lines[j])
                    if entry:
                        conditional_files.add(entry)
                    j += 1
    
    return conditional_files


def gather_expected_sources(block: CMakeSourceBlock, gitignore_spec: Any = None) -> List[str]:
    # First, find files that are in conditional blocks
    conditional_files = extract_conditional_files(block.cmake_path, block.variable)
    
    entries: Set[str] = set()
    for directory in block.directories:
        for source_file in directory.iter_files():
            if should_ignore_path(source_file):
                continue
            if is_ignored(source_file, gitignore_spec):
                continue
            
            # Exclude paths are relative to SOURCE_ROOT, so check against that.
            if relative_to_source(source_file) in block.exclude:
                continue

            # Generate paths relative to SOURCE_ROOT (src/) for consistency across the project
            # This matches the format used in editor_library.cmake, etc.
            rel_path = source_file.relative_to(SOURCE_ROOT)
            rel_path_str = str(rel_path).replace("\\", "/")

            # This check is imperfect if the conditional blocks have not been updated to use
            # SOURCE_ROOT relative paths. However, for the current issue, this is sufficient.
            if rel_path_str not in conditional_files:
                entries.add(rel_path_str)
    
    return sorted(entries)


def should_ignore_path(path: Path) -> bool:
    try:
        with path.open("r", encoding="utf-8") as handle:
            head = handle.read(256)
    except (OSError, UnicodeDecodeError):
        return False
    return BUILD_CLEANER_IGNORE_TOKEN in head


def extract_includes(file_path: Path) -> Set[str]:
    """Extract all #include statements from a source file."""
    includes = set()
    try:
        with file_path.open('r', encoding='utf-8') as f:
            for line in f:
                # Match #include "..." or #include <...>
                match = re.match(r'^\s*#include\s+[<"]([^>"]+)[>"]', line)
                if match:
                    includes.add(match.group(1))
    except (OSError, UnicodeDecodeError):
        pass
    return includes


def extract_symbols(file_path: Path) -> Set[str]:
    """Extract potential symbols/identifiers that might need headers."""
    symbols = set()
    try:
        with file_path.open('r', encoding='utf-8') as f:
            content = f.read()
            
            # Find namespace-qualified symbols (e.g., std::, absl::, ImGui::)
            namespace_symbols = re.findall(r'\b([a-zA-Z_]\w*::)', content)
            symbols.update(namespace_symbols)
            
            # Find common function calls that might need headers
            func_calls = re.findall(r'\b([A-Z][a-zA-Z0-9_]*)\s*\(', content)
            symbols.update(func_calls)
            
    except (OSError, UnicodeDecodeError):
        pass
    return symbols


def find_missing_headers(source: Path) -> List[str]:
    """Analyze a source file and suggest missing headers based on symbol usage."""
    if should_ignore_path(source):
        return []
    
    current_includes = extract_includes(source)
    symbols = extract_symbols(source)
    missing = []
    
    # Check for common headers based on symbol prefixes
    for symbol_prefix, headers in COMMON_HEADERS.items():
        if any(symbol_prefix in sym for sym in symbols):
            for header in headers:
                # Extract just the header name from angle brackets
                header_name = header.strip('<>')
                if header_name not in ' '.join(current_includes):
                    missing.append(header)
    
    return missing


def find_conditional_blocks_after(cmake_lines: List[str], end_idx: int, variable: str) -> List[str]:
    """
    Find conditional blocks (if/endif) that append to the variable after the main set() block.
    Returns lines that should be preserved.
    """
    conditional_lines = []
    idx = end_idx + 1
    
    while idx < len(cmake_lines):
        line = cmake_lines[idx]
        stripped = line.strip()
        
        # Stop at next major block or empty lines
        if not stripped:
            idx += 1
            continue
            
        # Check if this is a conditional that appends to our variable
        if stripped.startswith('if(') or stripped.startswith('if ('):
            # Look ahead to see if this block modifies our variable
            block_start = idx
            block_depth = 1
            modifies_var = False
            
            temp_idx = idx + 1
            while temp_idx < len(cmake_lines) and block_depth > 0:
                temp_line = cmake_lines[temp_idx].strip()
                if temp_line.startswith('if(') or temp_line.startswith('if '):
                    block_depth += 1
                elif temp_line.startswith('endif(') or temp_line == 'endif()':
                    block_depth -= 1
                    
                # Check if this block modifies our variable
                if f'APPEND {variable}' in temp_line or f'APPEND\n  {variable}' in cmake_lines[temp_idx]:
                    modifies_var = True
                    
                temp_idx += 1
            
            if modifies_var:
                # Include the entire conditional block
                conditional_lines.extend(cmake_lines[block_start:temp_idx])
                idx = temp_idx
                continue
            else:
                # This conditional doesn't touch our variable, stop scanning
                break
        else:
            # Hit something else, stop scanning
            break
            
        idx += 1
    
    return conditional_lines


def update_cmake_block(block: CMakeSourceBlock, dry_run: bool, gitignore_spec: Any = None) -> bool:
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

    expected_entries = gather_expected_sources(block, gitignore_spec)
    expected_set = set(expected_entries)

    if set(existing_entries) == expected_set:
        return False

    indent = "  "
    if first_entry_idx is not None:
        sample_line = block_slice[first_entry_idx]
        indent = sample_line[: len(sample_line) - len(sample_line.lstrip())]

    rebuilt_block = prelude + [f"{indent}{entry}" for entry in expected_entries] + postlude

    if dry_run:
        print(f"[DRY-RUN] Would update {block.cmake_path.relative_to(PROJECT_ROOT)} :: {block.variable}")
        return True

    cmake_lines[start_idx + 1 : end_idx] = rebuilt_block
    block.cmake_path.write_text("\n".join(cmake_lines) + "\n", encoding="utf-8")
    print(f"Updated {block.cmake_path.relative_to(PROJECT_ROOT)} :: {block.variable}")
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
    """Check if any line includes one of the header variants (with any path or quote style)."""
    # Extract just the header filenames for flexible matching
    header_names = {Path(variant).name for variant in header_variants}
    
    for line in lines:
        stripped = line.strip()
        if not stripped.startswith('#include'):
            continue
        
        # Extract the included filename from #include "..." or #include <...>
        match = re.match(r'^\s*#include\s+[<"]([^>"]+)[>"]', stripped)
        if match:
            included_path = match.group(1)
            included_name = Path(included_path).name
            
            # If this include references any of our header variants, consider it present
            if included_name in header_names:
                return True
    
    return False


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
    """
    Ensure a source file includes its corresponding header file.
    
    Skips files that:
    - Are explicitly ignored
    - Have no corresponding header
    - Already include their header (in any path format)
    - Are test files or main entry points (typically don't include own header)
    """
    if should_ignore_path(source):
        return False

    # Skip test files and main entry points (they typically don't need self-includes)
    source_name = source.name.lower()
    if any(pattern in source_name for pattern in ['_test.cc', '_main.cc', '_benchmark.cc', 'main.cc']):
        return False

    header = find_self_header(source)
    if header is None:
        # No corresponding header found - this is OK, not all sources have headers
        return False

    try:
        lines = source.read_text(encoding="utf-8").splitlines()
    except UnicodeDecodeError:
        return False

    # Generate header path relative to SOURCE_ROOT (project convention)
    try:
        header_rel_path = header.relative_to(SOURCE_ROOT)
        header_path_str = str(header_rel_path).replace("\\", "/")
    except ValueError:
        # Header is outside SOURCE_ROOT, just use filename
        header_path_str = header.name

    # Check if the header is already included (with any path format)
    header_variants = {
        header.name,  # Just filename
        header_path_str,  # SOURCE_ROOT-relative
        str(header.relative_to(source.parent)).replace("\\", "/") if source.parent != header.parent else header.name,  # Source-relative
    }

    if has_include(lines, header_variants):
        # Header is already included (possibly with different path)
        return False

    # Double-check: if this source file has very few lines or no code, skip it
    # (might be a stub or template file)
    code_lines = [l for l in lines if l.strip() and not l.strip().startswith('//') and not l.strip().startswith('/*')]
    if len(code_lines) < 3:
        return False

    # Use SOURCE_ROOT-relative path (project convention)
    include_line = f'#include "{header_path_str}"'

    insert_idx = find_insert_index(lines)
    lines.insert(insert_idx, include_line)

    if dry_run:
        rel = source.relative_to(PROJECT_ROOT)
        print(f"[DRY-RUN] Would insert self-header include into {rel}")
        return True

    source.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Inserted self-header include into {source.relative_to(PROJECT_ROOT)}")
    return True


def add_missing_headers(source: Path, dry_run: bool, iwyu_mode: bool) -> bool:
    """Add missing headers based on IWYU-style analysis."""
    if not iwyu_mode or should_ignore_path(source):
        return False
    
    missing_headers = find_missing_headers(source)
    if not missing_headers:
        return False
    
    try:
        lines = source.read_text(encoding="utf-8").splitlines()
    except UnicodeDecodeError:
        return False
    
    # Find where to insert the headers
    insert_idx = find_insert_index(lines)
    
    # Move past any existing includes to add new ones after them
    while insert_idx < len(lines) and lines[insert_idx].strip().startswith('#include'):
        insert_idx += 1
    
    # Insert missing headers
    for header in missing_headers:
        lines.insert(insert_idx, f'#include {header}')
        insert_idx += 1
    
    if dry_run:
        rel = source.relative_to(PROJECT_ROOT)
        print(f"[DRY-RUN] Would add missing headers to {rel}: {', '.join(missing_headers)}")
        return True
    
    source.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Added missing headers to {source.relative_to(PROJECT_ROOT)}: {', '.join(missing_headers)}")
    return True


def collect_source_files(config: List[CMakeSourceBlock], gitignore_spec: Any = None) -> Set[Path]:
    """Collect all source files from the given configuration, respecting .gitignore patterns."""
    managed_dirs: Set[Path] = set()
    
    for block in config:
        for directory in block.directories:
            managed_dirs.add(directory.path)

    result: Set[Path] = set()
    for directory in managed_dirs:
        if not directory.exists():
            continue
        for file_path in directory.rglob("*"):
            if file_path.is_file() and file_path.suffix in SUPPORTED_EXTENSIONS:
                if not is_ignored(file_path, gitignore_spec):
                    result.add(file_path)
    return result


def get_config(auto_discover: bool = False) -> List[CMakeSourceBlock]:
    """Get the full configuration, optionally including auto-discovered libraries."""
    # Always start with static config (all known libraries)
    config = list(STATIC_CONFIG)
    
    # Optionally add auto-discovered libraries, avoiding duplicates
    if auto_discover:
        discovered = discover_cmake_libraries()
        static_vars = {block.variable for block in STATIC_CONFIG}
        
        for block in discovered:
            if block.variable not in static_vars:
                config.append(block)
                print(f"  Auto-discovered: {block.variable} in {block.cmake_path.name}")
    
    return config


def run(dry_run: bool, cmake_only: bool, includes_only: bool, iwyu_mode: bool, auto_discover: bool) -> int:
    if cmake_only and includes_only:
        raise ValueError("Cannot use --cmake-only and --includes-only together")

    # Load .gitignore patterns
    gitignore_spec = load_gitignore()
    if gitignore_spec:
        print("✓ Loaded .gitignore patterns")

    changed = False
    
    # Get configuration (all libraries by default, with optional auto-discovery)
    config = get_config(auto_discover)
    
    if auto_discover:
        print(f"✓ Using {len(config)} library configurations (with auto-discovery)")
    else:
        print(f"✓ Using {len(config)} library configurations")

    if not includes_only:
        print("\n📋 Updating CMake source lists...")
        for block in config:
            changed |= update_cmake_block(block, dry_run, gitignore_spec)

    if not cmake_only:
        print("\n📝 Checking self-header includes...")
        source_files = collect_source_files(config, gitignore_spec)
        print(f"  Scanning {len(source_files)} source files")
        
        for source in source_files:
            changed |= ensure_self_header_include(source, dry_run)
            
        if iwyu_mode:
            print("\n🔍 Running IWYU-style header analysis...")
            for source in source_files:
                changed |= add_missing_headers(source, dry_run, iwyu_mode)

    if dry_run and not changed:
        print("\n✅ No changes required (dry-run)")
    elif not dry_run and not changed:
        print("\n✅ No changes required")
    elif dry_run:
        print("\n✅ Dry-run complete - use without --dry-run to apply changes")
    else:
        print("\n✅ All changes applied successfully")
    
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Maintain CMake source lists and ensure proper header includes (IWYU-style).",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Dry-run to see what would change:
  %(prog)s --dry-run
  
  # Auto-discover libraries and update CMake files:
  %(prog)s --auto-discover
  
  # Run IWYU-style header analysis:
  %(prog)s --iwyu
  
  # Update only CMake source lists:
  %(prog)s --cmake-only
  
  # Update only header includes:
  %(prog)s --includes-only
        """
    )
    parser.add_argument("--dry-run", action="store_true", 
                       help="Report prospective changes without editing files")
    parser.add_argument("--cmake-only", action="store_true", 
                       help="Only update CMake source lists")
    parser.add_argument("--includes-only", action="store_true", 
                       help="Only ensure self-header includes")
    parser.add_argument("--iwyu", action="store_true",
                       help="Run IWYU-style analysis to add missing headers")
    parser.add_argument("--auto-discover", action="store_true",
                       help="Auto-discover CMake library files (*.cmake, *_library.cmake)")
    args = parser.parse_args()

    try:
        return run(args.dry_run, args.cmake_only, args.includes_only, args.iwyu, args.auto_discover)
    except Exception as exc:  # pylint: disable=broad-except
        import traceback
        print(f"❌ build_cleaner failed: {exc}")
        if args.dry_run:  # Show traceback in dry-run mode for debugging
            traceback.print_exc()
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
