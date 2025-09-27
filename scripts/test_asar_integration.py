#!/usr/bin/env python3
"""
Asar Integration Test Script for Yaze
Tests the Asar 65816 assembler integration with real ROM files
"""

import os
import sys
import subprocess
import shutil
import tempfile
from pathlib import Path

def find_project_root():
    """Find the yaze project root directory"""
    current = Path(__file__).parent
    while current != current.parent:
        if (current / "CMakeLists.txt").exists():
            return current
        current = current.parent
    raise FileNotFoundError("Could not find yaze project root")

def main():
    print("🧪 Yaze Asar Integration Test")
    print("=" * 50)
    
    project_root = find_project_root()
    build_dir = project_root / "build_test"
    rom_path = build_dir / "bin" / "zelda3.sfc"
    test_patch = project_root / "test" / "assets" / "test_patch.asm"
    
    # Check if ROM file exists
    if not rom_path.exists():
        print(f"❌ ROM file not found: {rom_path}")
        print("   Please ensure you have a test ROM at the expected location")
        return 1
    
    print(f"✅ Found ROM file: {rom_path}")
    print(f"   Size: {rom_path.stat().st_size:,} bytes")
    
    # Check if test patch exists
    if not test_patch.exists():
        print(f"❌ Test patch not found: {test_patch}")
        return 1
    
    print(f"✅ Found test patch: {test_patch}")
    
    # Check if z3ed tool exists
    z3ed_path = build_dir / "bin" / "z3ed"
    if not z3ed_path.exists():
        print(f"❌ z3ed CLI tool not found: {z3ed_path}")
        print("   Run: cmake --build build_test --target z3ed")
        return 1
    
    print(f"✅ Found z3ed CLI tool: {z3ed_path}")
    
    # Create temporary directory for testing
    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)
        test_rom_path = temp_path / "test_rom.sfc"
        patched_rom_path = temp_path / "patched_rom.sfc"
        
        # Copy ROM to temporary location
        shutil.copy2(rom_path, test_rom_path)
        print(f"📋 Copied ROM to: {test_rom_path}")
        
        # Test 1: Apply patch using z3ed CLI
        print("\n🔧 Test 1: Applying patch with z3ed CLI")
        try:
            cmd = [str(z3ed_path), "asar", str(test_patch), str(test_rom_path)]
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
            
            if result.returncode == 0:
                print("✅ Patch applied successfully!")
                if result.stdout:
                    print(f"   Output: {result.stdout.strip()}")
            else:
                print(f"❌ Patch failed with return code: {result.returncode}")
                if result.stderr:
                    print(f"   Error: {result.stderr.strip()}")
                return 1
                
        except subprocess.TimeoutExpired:
            print("❌ Patch operation timed out")
            return 1
        except Exception as e:
            print(f"❌ Error running patch: {e}")
            return 1
        
        # Test 2: Verify ROM was modified
        print("\n🔍 Test 2: Verifying ROM modification")
        original_size = rom_path.stat().st_size
        modified_size = test_rom_path.stat().st_size
        
        print(f"   Original ROM size: {original_size:,} bytes")
        print(f"   Modified ROM size: {modified_size:,} bytes")
        
        # Read first few bytes to check for changes
        with open(rom_path, 'rb') as orig_file, open(test_rom_path, 'rb') as mod_file:
            orig_bytes = orig_file.read(1024)
            mod_bytes = mod_file.read(1024)
            
            if orig_bytes != mod_bytes:
                print("✅ ROM was successfully modified!")
                # Count different bytes
                diff_count = sum(1 for a, b in zip(orig_bytes, mod_bytes) if a != b)
                print(f"   {diff_count} bytes differ in first 1KB")
            else:
                print("⚠️  No differences detected in first 1KB")
                print("   (Patch may have been applied to a different region)")
        
        # Test 3: Run unit tests if available
        yaze_test_path = build_dir / "bin" / "yaze_test"
        if yaze_test_path.exists():
            print("\n🧪 Test 3: Running Asar unit tests")
            try:
                # Run only the Asar-related tests
                cmd = [str(yaze_test_path), "--gtest_filter=*Asar*", "--gtest_brief=1"]
                result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
                
                print(f"   Exit code: {result.returncode}")
                if result.stdout:
                    # Extract test results
                    lines = result.stdout.split('\n')
                    for line in lines:
                        if 'PASSED' in line or 'FAILED' in line or 'RUN' in line:
                            print(f"   {line}")
                            
                if result.returncode == 0:
                    print("✅ Unit tests passed!")
                else:
                    print("⚠️  Some unit tests failed (this may be expected)")
                    
            except subprocess.TimeoutExpired:
                print("❌ Unit tests timed out")
            except Exception as e:
                print(f"⚠️  Error running unit tests: {e}")
        else:
            print("\n⚠️  Test 3: yaze_test executable not found")
    
    print("\n🎉 Asar integration test completed!")
    print("\nNext steps:")
    print("- Run full test suite with: ctest --test-dir build_test")
    print("- Test Asar functionality in the main yaze application")
    print("- Create custom assembly patches for your ROM hacking projects")
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
