#!/usr/bin/env python3
import os
import re
import json
import argparse
from typing import Dict, List, Set, Tuple

class CodeNavigator:
    def __init__(self, root_dir: str):
        self.root_dir = os.path.expanduser(root_dir)
        self.labels: Dict[str, str] = {} # label -> filepath
        self.calls: List[Dict] = []      # {caller, callee, file, line}
        self.writes: List[Dict] = []     # {routine, address, file, line}
        
        # Regex patterns
        self.re_label = re.compile(r'^([A-Za-z_][A-Za-z0-9_]*):')
        self.re_call = re.compile(r'\b(JSL|JSR|JML)\s+([A-Za-z_][A-Za-z0-9_]*)')
        # Simple STA $XXXX or STA $XXXX,X parsing
        self.re_write = re.compile(r'\b(STA|STX|STY)\s+\$([0-9A-Fa-f]{4,6})')

    def scan_directory(self):
        """Recursively scan directory for .asm files."""
        print(f"Scanning {self.root_dir}...")
        for root, dirs, files in os.walk(self.root_dir):
            for file in files:
                if file.endswith(".asm"):
                    path = os.path.join(root, file)
                    self.parse_file(path)
        print(f"Found {len(self.labels)} labels, {len(self.calls)} calls, {len(self.writes)} writes.")

    def parse_file(self, filepath: str):
        rel_path = os.path.relpath(filepath, self.root_dir)
        current_routine = "unknown"
        
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            for i, line in enumerate(f):
                line_num = i + 1
                line = line.split(';')[0].strip() # Remove comments
                if not line: continue

                # Detect Label
                m_label = self.re_label.match(line)
                if m_label:
                    label = m_label.group(1)
                    self.labels[label] = rel_path
                    current_routine = label
                    continue

                # Detect Calls
                m_call = self.re_call.search(line)
                if m_call:
                    target = m_call.group(2)
                    self.calls.append({
                        "caller": current_routine,
                        "callee": target,
                        "file": rel_path,
                        "line": line_num
                    })

                # Detect Writes
                m_write = self.re_write.search(line)
                if m_write:
                    addr_str = m_write.group(2)
                    # Normalize address (remove 7E/7F prefix for WRAM mapping?)
                    # For now just keep as hex string
                    self.writes.append({
                        "routine": current_routine,
                        "address": addr_str,
                        "file": rel_path,
                        "line": line_num
                    })

    def export_graph(self) -> Dict:
        """Build call graph."""
        graph = {
            "nodes": [],
            "edges": []
        }
        
        nodes = set(self.labels.keys())
        for call in self.calls:
            nodes.add(call["caller"])
            nodes.add(call["callee"])
            
        for node in nodes:
            graph["nodes"].append({
                "id": node,
                "file": self.labels.get(node, "unknown")
            })
            
        for call in self.calls:
            graph["edges"].append({
                "source": call["caller"],
                "target": call["callee"],
                "type": "call"
            })
            
        return graph

    def find_callers(self, target_label: str) -> List[Dict]:
        return [c for c in self.calls if c["callee"] == target_label]

    def find_writes(self, address_pattern: str) -> List[Dict]:
        return [w for w in self.writes if address_pattern in w["address"]]

def main():
    parser = argparse.ArgumentParser(description="Codebase Navigator")
    parser.add_argument("root", help="Root directory of ASM files")
    subparsers = parser.add_subparsers(dest="command")
    
    graph_p = subparsers.add_parser("graph")
    graph_p.add_argument("--out", default="code_graph.json")
    
    callers_p = subparsers.add_parser("callers")
    callers_p.add_argument("label")
    
    writes_p = subparsers.add_parser("writes")
    writes_p.add_argument("address", help="Hex address (e.g. 7E0010)")

    args = parser.parse_args()
    
    nav = CodeNavigator(args.root)
    nav.scan_directory()
    
    if args.command == "graph":
        data = nav.export_graph()
        with open(args.out, 'w') as f:
            json.dump(data, f, indent=2)
        print(f"Graph saved to {args.out}")
        
    elif args.command == "callers":
        callers = nav.find_callers(args.label)
        print(f"Callers of {args.label}:")
        for c in callers:
            print(f"  {c['caller']} ({c['file']}:{c['line']})")
            
    elif args.command == "writes":
        writes = nav.find_writes(args.address)
        print(f"Writes to {args.address}:")
        for w in writes:
            print(f"  {w['routine']} ({w['file']}:{w['line']})")

if __name__ == "__main__":
    main()
