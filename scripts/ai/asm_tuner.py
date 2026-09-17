#!/usr/bin/env python3
"""ROMHack ASM tuner: syntax-verify, style-check, apply, and revert z3asm patches.

Environment overrides:
    Z3ASM_BIN   Explicit path to the z3asm binary.
    Z3DK_ROOT   Root of a z3dk checkout (default: the yaze repo's sibling ../z3dk).

Resolution order for z3asm is --z3asm, then Z3ASM_BIN, then PATH, then the
usual build outputs under Z3DK_ROOT.
"""
import argparse
import subprocess
import os
import re
import shutil
import sys
import tempfile
from typing import Any, Dict, List, Optional

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
Z3DK_ROOT = os.environ.get(
    "Z3DK_ROOT", os.path.abspath(os.path.join(REPO_ROOT, "..", "z3dk"))
)

# Mirrors the candidate layout used by scripts/z3disasm.
_Z3ASM_CANDIDATES = (
    os.path.join("build", "src", "z3asm", "bin", "z3asm"),
    os.path.join("build", "bin", "z3asm"),
    os.path.join("build", "bin", "Debug", "z3asm"),
    os.path.join("build", "bin", "RelWithDebInfo", "z3asm"),
    os.path.join("build", "bin", "Release", "z3asm"),
)


def resolve_z3asm(explicit: Optional[str] = None) -> Optional[str]:
    """Return the z3asm binary path, or None when it cannot be located."""
    if explicit:
        return explicit

    from_env = os.environ.get("Z3ASM_BIN")
    if from_env:
        return from_env

    on_path = shutil.which("z3asm")
    if on_path:
        return on_path

    for relative in _Z3ASM_CANDIDATES:
        candidate = os.path.join(Z3DK_ROOT, relative)
        if os.access(candidate, os.X_OK):
            return candidate

    return None


class AsmTuner:
    def __init__(self, z3asm_path: Optional[str], rom_path: Optional[str] = None):
        self.z3asm_path = z3asm_path
        self.rom_path = rom_path

    def verify_syntax(self, asm_content: str) -> Dict[str, Any]:
        """
        Run z3asm on the provided ASM content to check for syntax errors.
        Returns a dict with 'valid' (bool), 'errors' (list), 'output' (str).
        """
        with tempfile.NamedTemporaryFile(mode='w', suffix='.asm', delete=False) as tmp_asm:
            tmp_asm.write(asm_content)
            tmp_asm_path = tmp_asm.name

        # Create a dummy ROM if none provided, or copy the existing one to avoid modification?
        # z3asm usually needs a ROM to patch onto.
        # If we just want syntax check, we can try to patch a dummy file.
        
        with tempfile.NamedTemporaryFile(suffix='.sfc', delete=False) as tmp_rom:
            if self.rom_path and os.path.exists(self.rom_path):
                # Copy input ROM to temp
                with open(self.rom_path, 'rb') as src_rom:
                    tmp_rom.write(src_rom.read())
            else:
                # Create a blank 2MB file
                tmp_rom.write(b'\x00' * (2 * 1024 * 1024))
            tmp_rom_path = tmp_rom.name

        try:
            # Run z3asm
            # z3asm <patch.asm> <rom.sfc>
            cmd = [self.z3asm_path, tmp_asm_path, tmp_rom_path]
            result = subprocess.run(cmd, capture_output=True, text=True)
            
            output = result.stdout + result.stderr
            valid = result.returncode == 0
            
            errors = []
            if not valid:
                # Parse standard asar errors: "file.asm:line: error: message"
                for line in output.splitlines():
                    if "error:" in line or "warning:" in line:
                        errors.append(line.strip())

            return {
                "valid": valid,
                "errors": errors,
                "output": output
            }

        finally:
            if os.path.exists(tmp_asm_path):
                os.remove(tmp_asm_path)
            if os.path.exists(tmp_rom_path):
                os.remove(tmp_rom_path)

    def apply_patch(self, asm_file: str) -> bool:
        """
        Apply an ASM patch to the ROM. Creates a backup (.bak) first.
        """
        if not self.rom_path or not os.path.exists(self.rom_path):
            print(f"Error: ROM not found at {self.rom_path}")
            return False

        # Backup
        bak_path = self.rom_path + ".bak"
        if not os.path.exists(bak_path):
            print(f"Creating backup: {bak_path}")
            shutil.copy2(self.rom_path, bak_path)
        else:
            print(f"Backup already exists: {bak_path}")

        # Patch
        print(f"Applying {asm_file} to {self.rom_path}...")
        cmd = [self.z3asm_path, asm_file, self.rom_path]
        result = subprocess.run(cmd, capture_output=True, text=True)
        
        if result.returncode == 0:
            print("✅ Patch applied successfully.")
            return True
        else:
            print("❌ Patch failed:")
            print(result.stdout + result.stderr)
            return False

    def revert_rom(self) -> bool:
        """
        Restore the ROM from the .bak file.
        """
        if not self.rom_path:
            print("Error: No ROM path configured.")
            return False

        bak_path = self.rom_path + ".bak"
        if not os.path.exists(bak_path):
            print(f"Error: No backup found at {bak_path}")
            return False

        print(f"Restoring {self.rom_path} from {bak_path}...")
        shutil.copy2(bak_path, self.rom_path)
        print("✅ Revert complete.")
        return True

    def check_style(self, asm_content: str) -> List[str]:
        """
        Check for Oracle of Secrets ASM style conventions.
        """
        violations = []
        lines = asm_content.splitlines()
        
        for i, line in enumerate(lines):
            lnum = i + 1
            content = line.split(';')[0].strip() # Ignore comments
            
            # 1. Long Addressing for WRAM ($7E/7F)
            # Regex: STA $7E... or LDA $7E... without .l or .long
            # Match instruction followed by $7E or $7F
            # Exclusion: STA.l, LDA.l
            if re.search(r'\b(LDA|STA|STX|STY|LDX|LDY|ADC|SBC|CMP|ORA|AND|EOR|BIT)\s+\$(7E|7F)[0-9A-F]{4}\b', content, re.IGNORECASE):
                # Check if it has .l suffix
                if not re.search(r'\b(LDA|STA|STX|STY|LDX|LDY|ADC|SBC|CMP|ORA|AND|EOR|BIT)\.l', content, re.IGNORECASE):
                     violations.append(f"Line {lnum}: Short addressing used for WRAM ($7E/7F). Use long addressing (e.g., STA.l $7E0010). Content: {content}")

            # 2. Check for missing bank wrappers in Routines
            # Heuristic: If label ends with ':' and looks like a routine start, check for PHB/PHK/PLB nearby?
            # This is hard to enforce strictly on snippets, skipping for now.

            # 3. Direct Page assumptions
            # If accessing $00-$FF, warn if DP isn't known?
            # Too noisy.

        return violations

def main():
    parser = argparse.ArgumentParser(
        description="ROMHack ASM Tuner",
        epilog=(
            "z3asm is resolved from --z3asm, then $Z3ASM_BIN, then PATH, then "
            f"build outputs under $Z3DK_ROOT (currently {Z3DK_ROOT})."
        ),
    )
    subparsers = parser.add_subparsers(dest="command")

    z3asm_help = "Path to z3asm binary (default: $Z3ASM_BIN, PATH, then $Z3DK_ROOT)"

    verify_p = subparsers.add_parser("verify")
    verify_p.add_argument("file", help="Path to ASM file")
    verify_p.add_argument("--rom", help="Path to base ROM (optional)", default=None)
    verify_p.add_argument("--z3asm", help=z3asm_help, default=None)

    style_p = subparsers.add_parser("style")
    style_p.add_argument("file", help="Path to ASM file")

    apply_p = subparsers.add_parser("apply")
    apply_p.add_argument("file", help="Path to ASM file")
    apply_p.add_argument("--rom", help="Path to ROM file", required=True)
    apply_p.add_argument("--z3asm", help=z3asm_help, default=None)

    revert_p = subparsers.add_parser("revert")
    revert_p.add_argument("--rom", help="Path to ROM file", required=True)

    args = parser.parse_args()

    rom = getattr(args, 'rom', None)
    z3asm = resolve_z3asm(getattr(args, 'z3asm', None))
    if z3asm is None and args.command in ("verify", "apply"):
        print(
            "z3asm not found. Set Z3ASM_BIN, put z3asm on PATH, or build it:\n"
            f"  cmake --build {os.path.join(Z3DK_ROOT, 'build')} --target z3asm",
            file=sys.stderr,
        )
        sys.exit(1)

    tuner = AsmTuner(z3asm, rom)

    if args.command == "verify":
        if not os.path.exists(args.file):
            print(f"File not found: {args.file}")
            sys.exit(1)
        
        with open(args.file, 'r') as f:
            content = f.read()
            
        print(f"Verifying {args.file}...")
        result = tuner.verify_syntax(content)
        
        if result["valid"]:
            print("✅ Syntax Valid")
        else:
            print("❌ Syntax Errors:")
            for err in result["errors"]:
                print(f"  {err}")
            # print("\nFull Output:\n", result["output"])

        print("\nStyle Check:")
        style_errors = tuner.check_style(content)
        if not style_errors:
            print("✅ No Style Violations")
        else:
            for err in style_errors:
                print(f"  ⚠️ {err}")

    elif args.command == "style":
        if not os.path.exists(args.file):
            print(f"File not found: {args.file}")
            sys.exit(1)
        with open(args.file, 'r') as f:
            content = f.read()
        errors = tuner.check_style(content)
        for err in errors:
            print(err)

    elif args.command == "apply":
        if not os.path.exists(args.file):
            print(f"File not found: {args.file}")
            sys.exit(1)
        tuner.apply_patch(args.file)

    elif args.command == "revert":
        tuner.revert_rom()

    else:
        parser.print_help()


if __name__ == "__main__":
    main()
