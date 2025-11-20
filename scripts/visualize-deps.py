#!/usr/bin/env python3
"""
Dependency Graph Visualizer
Parses CMake targets and generates dependency graphs

Usage:
    python3 scripts/visualize-deps.py [build_directory] [--format FORMAT]

Formats:
    - graphviz: DOT format for graphviz (default)
    - mermaid: Mermaid diagram format
    - text: Simple text tree

Exit codes:
    0 - Success
    1 - Error (build directory not found, etc.)
"""

import os
import sys
import json
import argparse
import re
from pathlib import Path
from typing import Dict, Set, List, Tuple
from collections import defaultdict


class Colors:
    """ANSI color codes"""
    RESET = '\033[0m'
    RED = '\033[31m'
    GREEN = '\033[32m'
    YELLOW = '\033[33m'
    BLUE = '\033[34m'
    MAGENTA = '\033[35m'
    CYAN = '\033[36m'


class DependencyGraph:
    """Parse and analyze CMake dependency graph"""

    def __init__(self, build_dir: Path):
        self.build_dir = build_dir
        self.targets: Dict[str, Set[str]] = defaultdict(set)
        self.target_types: Dict[str, str] = {}
        self.circular_deps: List[List[str]] = []

    def parse_cmake_files(self):
        """Parse CMakeLists.txt files to extract targets and dependencies"""
        print(f"{Colors.BLUE}Parsing CMake configuration...{Colors.RESET}")

        # Try to parse from CMake's dependency info
        dep_info_dir = self.build_dir / "CMakeFiles" / "TargetDirectories.txt"

        if dep_info_dir.exists():
            with open(dep_info_dir, 'r') as f:
                for line in f:
                    line = line.strip()
                    if line:
                        # Extract target name from path
                        match = re.search(r'CMakeFiles/([^/]+)\.dir', line)
                        if match:
                            target_name = match.group(1)
                            self.targets[target_name] = set()
                            self.target_types[target_name] = "UNKNOWN"

        # Try to extract more info from compile_commands.json
        compile_commands = self.build_dir / "compile_commands.json"
        if compile_commands.exists():
            try:
                with open(compile_commands, 'r') as f:
                    commands = json.load(f)

                for cmd in commands:
                    file_path = cmd.get('file', '')
                    # Try to infer target from file path
                    if '/src/' in file_path:
                        parts = file_path.split('/src/')[-1].split('/')
                        if len(parts) > 1:
                            target = parts[0]
                            if target not in self.targets:
                                self.targets[target] = set()
                                self.target_types[target] = "LIBRARY"

            except json.JSONDecodeError:
                print(f"{Colors.YELLOW}⚠ Could not parse compile_commands.json{Colors.RESET}")

        # Parse dependency information from generated cmake files
        self._parse_cmake_depends()

        print(f"{Colors.GREEN}✓ Found {len(self.targets)} targets{Colors.RESET}")

    def _parse_cmake_depends(self):
        """Parse CMake depend.make files for dependency information"""
        cmake_files_dir = self.build_dir / "CMakeFiles"

        if not cmake_files_dir.exists():
            return

        # Look for depend.make files
        for target_dir in cmake_files_dir.glob("*.dir"):
            target_name = target_dir.name.replace('.dir', '')

            depend_make = target_dir / "depend.make"
            if depend_make.exists():
                try:
                    with open(depend_make, 'r') as f:
                        content = f.read()

                    # Extract dependencies from depend.make
                    # Format: target_name.dir/file.cc.o: path/to/header.h
                    for line in content.split('\n'):
                        if ':' in line and not line.startswith('#'):
                            parts = line.split(':')
                            if len(parts) >= 2:
                                deps = parts[1].strip()
                                # Look for other target dependencies
                                for other_target in self.targets.keys():
                                    if other_target in deps and other_target != target_name:
                                        self.targets[target_name].add(other_target)

                except Exception as e:
                    print(f"{Colors.YELLOW}⚠ Error parsing {depend_make}: {e}{Colors.RESET}")

        # Also check link.txt for library dependencies
        for target_dir in cmake_files_dir.glob("*.dir"):
            target_name = target_dir.name.replace('.dir', '')
            link_txt = target_dir / "link.txt"

            if link_txt.exists():
                try:
                    with open(link_txt, 'r') as f:
                        link_cmd = f.read()

                    # Parse linked libraries
                    for other_target in self.targets.keys():
                        if other_target in link_cmd and other_target != target_name:
                            self.targets[target_name].add(other_target)

                except Exception:
                    pass

    def detect_circular_dependencies(self) -> List[List[str]]:
        """Detect circular dependencies using DFS"""
        print(f"{Colors.BLUE}Checking for circular dependencies...{Colors.RESET}")

        visited = set()
        rec_stack = set()
        cycles = []

        def dfs(node: str, path: List[str]):
            visited.add(node)
            rec_stack.add(node)
            path.append(node)

            for neighbor in self.targets.get(node, []):
                if neighbor not in visited:
                    dfs(neighbor, path.copy())
                elif neighbor in rec_stack:
                    # Found a cycle
                    cycle_start = path.index(neighbor)
                    cycle = path[cycle_start:] + [neighbor]
                    cycles.append(cycle)

            rec_stack.remove(node)

        for target in self.targets:
            if target not in visited:
                dfs(target, [])

        self.circular_deps = cycles

        if cycles:
            print(f"{Colors.RED}✗ Found {len(cycles)} circular dependencies{Colors.RESET}")
            for cycle in cycles:
                print(f"  {' -> '.join(cycle)}")
        else:
            print(f"{Colors.GREEN}✓ No circular dependencies detected{Colors.RESET}")

        return cycles

    def generate_graphviz(self, output_file: Path = None):
        """Generate GraphViz DOT format"""
        print(f"{Colors.BLUE}Generating GraphViz diagram...{Colors.RESET}")

        dot = ["digraph Dependencies {"]
        dot.append("  rankdir=LR;")
        dot.append("  node [shape=box, style=rounded];")
        dot.append("")

        # Define node styles
        dot.append("  // Node definitions")
        for target, target_type in self.target_types.items():
            if target_type == "EXECUTABLE":
                color = "lightblue"
            elif target_type == "LIBRARY":
                color = "lightgreen"
            else:
                color = "lightgray"

            safe_name = target.replace("-", "_").replace("::", "_")
            dot.append(f'  {safe_name} [label="{target}", fillcolor={color}, style="rounded,filled"];')

        dot.append("")
        dot.append("  // Dependencies")

        # Add edges
        for target, deps in self.targets.items():
            safe_target = target.replace("-", "_").replace("::", "_")
            for dep in deps:
                safe_dep = dep.replace("-", "_").replace("::", "_")

                # Highlight circular dependencies in red
                is_circular = any(
                    target in cycle and dep in cycle
                    for cycle in self.circular_deps
                )

                if is_circular:
                    dot.append(f'  {safe_target} -> {safe_dep} [color=red, penwidth=2];')
                else:
                    dot.append(f'  {safe_target} -> {safe_dep};')

        dot.append("}")

        result = "\n".join(dot)

        if output_file:
            output_file.write_text(result)
            print(f"{Colors.GREEN}✓ GraphViz diagram written to {output_file}{Colors.RESET}")
        else:
            print(result)

        return result

    def generate_mermaid(self, output_file: Path = None):
        """Generate Mermaid diagram format"""
        print(f"{Colors.BLUE}Generating Mermaid diagram...{Colors.RESET}")

        mermaid = ["graph LR"]

        # Add nodes and edges
        for target, deps in self.targets.items():
            safe_target = target.replace("-", "_").replace("::", "_")

            for dep in deps:
                safe_dep = dep.replace("-", "_").replace("::", "_")

                # Highlight circular dependencies
                is_circular = any(
                    target in cycle and dep in cycle
                    for cycle in self.circular_deps
                )

                if is_circular:
                    mermaid.append(f'  {safe_target}-->|CIRCULAR|{safe_dep}')
                    mermaid.append(f'  style {safe_target} fill:#ff6b6b')
                    mermaid.append(f'  style {safe_dep} fill:#ff6b6b')
                else:
                    mermaid.append(f'  {safe_target}-->{safe_dep}')

        result = "\n".join(mermaid)

        if output_file:
            output_file.write_text(result)
            print(f"{Colors.GREEN}✓ Mermaid diagram written to {output_file}{Colors.RESET}")
        else:
            print(result)

        return result

    def generate_text_tree(self, output_file: Path = None):
        """Generate simple text tree representation"""
        print(f"{Colors.BLUE}Generating text tree...{Colors.RESET}")

        lines = []
        visited = set()

        def print_tree(target: str, indent: int = 0, prefix: str = ""):
            if target in visited:
                lines.append(f"{prefix}├── {target} (circular)")
                return

            visited.add(target)
            deps = list(self.targets.get(target, []))

            lines.append(f"{prefix}├── {target}")

            for i, dep in enumerate(deps):
                is_last = i == len(deps) - 1
                new_prefix = prefix + ("    " if is_last else "│   ")
                print_tree(dep, indent + 1, new_prefix)

        # Find root targets (targets with no incoming dependencies)
        all_deps = set()
        for deps in self.targets.values():
            all_deps.update(deps)

        roots = [t for t in self.targets.keys() if t not in all_deps]

        if not roots:
            # If no clear roots, just use all targets
            roots = list(self.targets.keys())

        lines.append("Dependency Tree:")
        for root in roots[:10]:  # Limit to first 10 roots
            visited = set()
            print_tree(root)
            lines.append("")

        result = "\n".join(lines)

        if output_file:
            output_file.write_text(result)
            print(f"{Colors.GREEN}✓ Text tree written to {output_file}{Colors.RESET}")
        else:
            print(result)

        return result

    def print_statistics(self):
        """Print graph statistics"""
        print(f"\n{Colors.BLUE}=== Dependency Statistics ==={Colors.RESET}")

        total_targets = len(self.targets)
        total_edges = sum(len(deps) for deps in self.targets.values())
        avg_deps = total_edges / total_targets if total_targets > 0 else 0

        print(f"Total targets: {total_targets}")
        print(f"Total dependencies: {total_edges}")
        print(f"Average dependencies per target: {avg_deps:.2f}")

        # Find most connected targets
        dep_counts = [(t, len(deps)) for t, deps in self.targets.items()]
        dep_counts.sort(key=lambda x: x[1], reverse=True)

        print(f"\n{Colors.CYAN}Most connected targets:{Colors.RESET}")
        for target, count in dep_counts[:5]:
            print(f"  {target}: {count} dependencies")

        # Find targets with no dependencies
        isolated = [t for t, deps in self.targets.items() if len(deps) == 0]
        if isolated:
            print(f"\n{Colors.YELLOW}Isolated targets (no dependencies):{Colors.RESET}")
            for target in isolated[:10]:
                print(f"  {target}")


def main():
    parser = argparse.ArgumentParser(description="Visualize CMake dependency graph")
    parser.add_argument(
        "build_dir",
        nargs="?",
        default="build",
        help="Build directory (default: build)"
    )
    parser.add_argument(
        "--format",
        choices=["graphviz", "mermaid", "text"],
        default="graphviz",
        help="Output format (default: graphviz)"
    )
    parser.add_argument(
        "--output",
        "-o",
        type=Path,
        help="Output file (default: stdout)"
    )
    parser.add_argument(
        "--stats",
        action="store_true",
        help="Show statistics"
    )

    args = parser.parse_args()

    build_dir = Path(args.build_dir)

    if not build_dir.exists():
        print(f"{Colors.RED}✗ Build directory not found: {build_dir}{Colors.RESET}")
        print("Run cmake configure first: cmake --preset <preset-name>")
        sys.exit(1)

    if not (build_dir / "CMakeCache.txt").exists():
        print(f"{Colors.RED}✗ CMakeCache.txt not found in {build_dir}{Colors.RESET}")
        print("Configuration incomplete - run cmake configure first")
        sys.exit(1)

    print(f"{Colors.BLUE}=== CMake Dependency Visualizer ==={Colors.RESET}")
    print(f"Build directory: {build_dir}\n")

    graph = DependencyGraph(build_dir)
    graph.parse_cmake_files()
    graph.detect_circular_dependencies()

    if args.stats:
        graph.print_statistics()

    # Generate output
    output_file = args.output

    if args.format == "graphviz":
        if not output_file:
            output_file = Path("dependencies.dot")
        graph.generate_graphviz(output_file)
        print(f"\nTo render: dot -Tpng {output_file} -o dependencies.png")

    elif args.format == "mermaid":
        if not output_file:
            output_file = Path("dependencies.mmd")
        graph.generate_mermaid(output_file)
        print(f"\nView at: https://mermaid.live/edit")

    elif args.format == "text":
        graph.generate_text_tree(output_file)

    print(f"\n{Colors.GREEN}✓ Dependency analysis complete{Colors.RESET}")


if __name__ == "__main__":
    main()
