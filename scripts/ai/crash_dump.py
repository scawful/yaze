#!/usr/bin/env python3
import sys
import os
import json
import argparse
import time
import subprocess
from datetime import datetime
from typing import Dict, List, Optional

# Add Oracle scripts to path
ORACLE_SCRIPTS = os.path.expanduser("~/src/hobby/oracle-of-secrets/scripts")
sys.path.append(ORACLE_SCRIPTS)

from mesen2_client_lib.client import OracleDebugClient

class CrashInvestigator:
    def __init__(self, z3ed_path: str, rom_path: str):
        self.client = OracleDebugClient()
        self.z3ed_path = os.path.expanduser(z3ed_path)
        self.rom_path = os.path.expanduser(rom_path)
        self.report_dir = "crash_reports"
        os.makedirs(self.report_dir, exist_ok=True)

    def ensure_connected(self):
        if not self.client.ensure_connected():
            print("Error: Could not connect to Mesen2.")
            sys.exit(1)

    def capture_trace(self, count: int = 1000) -> List[Dict]:
        """Capture the execution trace from Mesen2."""
        print(f"Capturing last {count} frames of trace...")
        # Note: 'TRACE' command in Mesen2-OoS returns list of {pc, a, x, y, sp, p}
        try:
            # Older bridge signatures omit params; tolerate both.
            res = self.client.bridge.send_command("TRACE", {"count": count})
        except TypeError:
            res = self.client.bridge.send_command("TRACE")
        if not res.get("success"):
            print(f"Error capturing trace: {res.get('error')}")
            return []
        
        data = res.get("data")
        if isinstance(data, str):
            try:
                return json.loads(data)
            except json.JSONDecodeError:
                print("Error: Invalid JSON from TRACE command")
                return []
        return data if isinstance(data, list) else []

    def resolve_symbol(self, address: int) -> Dict:
        """Resolve address to symbol using z3ed."""
        # Cache results? For a crash dump, speed isn't critical.
        cmd = [
            self.z3ed_path, 
            "rom-resolve-address", 
            f"--address=0x{address:X}", 
            f"--rom={self.rom_path}", 
            "--format=json"
        ]
        
        try:
            res = subprocess.run(cmd, capture_output=True, text=True, check=False)
            if res.returncode != 0:
                return {"status": "error", "message": res.stderr}
            return json.loads(res.stdout)
        except Exception as e:
            return {"status": "error", "message": str(e)}

    def analyze_trace(self, trace: List[Dict]) -> List[Dict]:
        """Annotate trace with symbols."""
        annotated = []
        print("Resolving symbols...")
        # Resolve unique PCs to save time
        unique_pcs = set(entry.get("pc", 0) for entry in trace)
        symbol_map = {}
        
        for i, pc in enumerate(unique_pcs):
            print(f"  Resolving 0x{pc:06X} ({i+1}/{len(unique_pcs)})...", end="\r")
            info = self.resolve_symbol(pc)
            if info.get("status") == "success":
                symbol_map[pc] = info
        print("\nSymbol resolution complete.")

        for entry in trace:
            pc = entry.get("pc", 0)
            sym = symbol_map.get(pc)
            
            new_entry = entry.copy()
            if sym:
                new_entry["symbol"] = sym.get("name", "")
                new_entry["file"] = sym.get("file", "")
                new_entry["line"] = sym.get("line", "")
                new_entry["offset"] = sym.get("offset", "") # if nearest match
            annotated.append(new_entry)
            
        return annotated

    def get_code_snippet(self, file_path: str, line_num: int, context: int = 5) -> str:
        """Read a snippet of code from an ASM file."""
        if not file_path or not os.path.exists(file_path):
            return ""
        
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                lines = f.readlines()
                
            start = max(0, line_num - context - 1)
            end = min(len(lines), line_num + context)
            
            snippet = []
            for i in range(start, end):
                prefix = ">>" if i == line_num - 1 else "  "
                snippet.append(f"{prefix} {i+1:4d}: {lines[i].rstrip()}")
            
            return "\n".join(snippet)
        except Exception as e:
            return f"Error reading snippet: {e}"

    def save_report(self, trace: List[Dict], state: Dict, context: str = "manual"):
        """Generate Markdown crash report."""
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"{self.report_dir}/crash_{timestamp}.md"
        
        # Snapshot CPU state if not provided
        if not state:
            try:
                cpu = self.client.bridge.send_command("CPU")
                if cpu.get("success"):
                    state = cpu.get("data", {})
            except Exception:
                pass

        # Snapshot top of stack for context
        stack_bytes = []
        try:
            sp_str = state.get("sp", state.get("stackPointer", None))
            sp = int(sp_str, 16) if isinstance(sp_str, str) else None
            if sp is not None:
                base = 0x7E0000 + sp - 0x20  # pull 32 bytes above SP
                base = max(0x7E0000, base)
                raw = []
                for i in range(64):
                    raw.append(self.client.bridge.read_memory(base + i))
                stack_bytes = raw
        except Exception:
            stack_bytes = []

        # Identify the probable crash site (the last frame in the trace)
        crash_frame = trace[-1] if trace else {}
        crash_file = crash_frame.get("file")
        crash_line = crash_frame.get("line")
        
        with open(filename, "w") as f:
            f.write(f"# Crash Report: {timestamp}\n\n")
            f.write(f"**Context**: {context}\n")
            f.write(f"**ROM**: `{self.rom_path}`\n\n")
            if stack_bytes:
                f.write("## 0a. Stack (top ~64 bytes)\n```\n")
                for i, b in enumerate(stack_bytes):
                    if i % 16 == 0:
                        f.write(f"\n0x{(i):02X}: ")
                    f.write(f"{b:02X} ")
                f.write("\n```\n\n")
            
            if crash_file and crash_line:
                f.write(f"## 0. Crash Site: `{os.path.basename(crash_file)}:{crash_line}`\n")
                f.write("```asm\n")
                f.write(self.get_code_snippet(crash_file, int(crash_line)))
                f.write("\n```\n\n")

            f.write("## 1. Machine State (At Crash)\n")
            f.write("```json\n")
            f.write(json.dumps(state, indent=2))
            f.write("\n```\n\n")
            
            f.write("## 2. Execution Trace (Last 100 Entries)\n")
            f.write("| Frame | PC | Symbol | Source | A | X | Y | P |\n")
            f.write("|-------|----|--------|--------|---|---|---|---|")
            
            # Show last 100 frames reverse order (newest first)
            for i, entry in enumerate(reversed(trace[-100:])):
                pc = entry.get("pc", 0)
                sym = entry.get("symbol", "")
                if entry.get("offset"):
                    sym += f"+{entry.get('offset')}"
                src = f"{os.path.basename(entry.get('file', ''))}:{entry.get('line', '')}" if entry.get('file') else ""
                
                # Format registers
                a = f"{entry.get('a', 0):02X}"
                x = f"{entry.get('x', 0):02X}"
                y = f"{entry.get('y', 0):02X}"
                p = f"{entry.get('p', 0):02X}"
                
                f.write(f"| -{i} | `{pc:06X}` | `{sym}` | `{src}` | {a} | {x} | {y} | {p} |\n")
                
            f.write("\n## 3. Analysis\n")
            f.write("*Auto-generated section for LLM analysis*\n")
            
        print(f"Report saved to {filename}")

    def monitor(self):
        """Poll for crash conditions."""
        print("Monitoring for crashes (Ctrl+C to stop)...")
        last_state = "running"
        try:
            while True:
                # Check run state
                run_state = self.client.get_run_state()
                paused = run_state.get("paused", False)
                
                if paused and last_state == "running":
                    # We just paused. Check why.
                    # Usually Mesen pauses on breakpoint.
                    print("\nEmulator Paused. Capturing snapshot...")
                    self.dump("paused_event")
                    
                    # Wait for resume
                    while self.client.is_paused():
                        time.sleep(1)
                        
                last_state = "paused" if paused else "running"
                time.sleep(0.5)
        except KeyboardInterrupt:
            print("\nStopping monitor.")

    def dump(self, reason: str):
        """Perform immediate dump."""
        # 1. Get State
        cpu = self.client.get_cpu_state() # Registers
        # 2. Get Trace
        raw_trace = self.capture_trace()
        # 3. Analyze
        annotated_trace = self.analyze_trace(raw_trace)
        # 4. Report
        self.save_report(annotated_trace, cpu, reason)

def main():
    parser = argparse.ArgumentParser(description="Crash Investigator")
    parser.add_argument("--z3ed", default=os.path.expanduser("~/src/hobby/yaze/build_ai/bin/Debug/z3ed"), help="Path to z3ed binary")
    parser.add_argument("--rom", default=os.path.expanduser("~/src/hobby/oracle-of-secrets/Roms/oos168x.sfc"), help="Path to ROM")
    
    subparsers = parser.add_subparsers(dest="command")
    subparsers.add_parser("monitor", help="Monitor for crashes/pauses")
    subparsers.add_parser("dump", help="Trigger immediate dump")
    
    args = parser.parse_args()
    
    if not os.path.exists(args.z3ed):
        print(f"Error: z3ed not found at {args.z3ed}")
        sys.exit(1)
        
    investigator = CrashInvestigator(args.z3ed, args.rom)
    investigator.ensure_connected()
    
    if args.command == "monitor":
        investigator.monitor()
    elif args.command == "dump":
        investigator.dump("manual_trigger")
    else:
        parser.print_help()

if __name__ == "__main__":
    main()
