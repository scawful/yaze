#!/usr/bin/env python3
import sys
import os
import json
import base64
import argparse
import time
from io import BytesIO
from typing import Optional, Tuple

# Add Oracle scripts to path to find mesen2_client_lib
ORACLE_SCRIPTS = os.path.expanduser("~/src/hobby/oracle-of-secrets/scripts")
sys.path.append(ORACLE_SCRIPTS)

from mesen2_client_lib.client import OracleDebugClient

try:
    import cv2
    import numpy as np
    from PIL import Image
except ImportError as e:
    print(f"Error: Missing dependencies. {e}")
    print("Please install: pip install opencv-python Pillow")
    sys.exit(1)

class VisualVerifier:
    def __init__(self, master_dir: str = "assets/visual_masters"):
        self.client = OracleDebugClient()
        self.master_dir = os.path.expanduser(master_dir)
        os.makedirs(self.master_dir, exist_ok=True)

    def capture_screenshot(self) -> Image.Image:
        """Capture screenshot from Mesen2 and return as PIL Image."""
        if not self.client.ensure_connected():
            raise ConnectionError("Could not connect to Mesen2.")
        
        res = self.client.bridge.send_command("SCREENSHOT")
        if not res.get("success"):
            raise RuntimeError(f"Screenshot failed: {res.get('error')}")
        
        # Data is base64 encoded PNG
        img_data = base64.b64decode(res["data"])
        return Image.open(BytesIO(img_data))

    def save_master(self, tag: str):
        """Capture current screen and save as golden master."""
        img = self.capture_screenshot()
        path = os.path.join(self.master_dir, f"{tag}.png")
        img.save(path)
        print(f"Saved master screenshot to {path}")

    def verify(self, tag: str, threshold: float = 0.95) -> Tuple[bool, float, str]:
        """Compare current screen against golden master."""
        master_path = os.path.join(self.master_dir, f"{tag}.png")
        if not os.path.exists(master_path):
            return False, 0.0, f"Master not found for tag: {tag}"

        master_img = cv2.imread(master_path)
        current_pil = self.capture_screenshot()
        
        # Convert PIL to OpenCV format
        current_img = cv2.cvtColor(np.array(current_pil), cv2.COLOR_RGB2BGR)

        if master_img.shape != current_img.shape:
            # Resize if needed? Or fail.
            # SNES resolution is usually fixed, but aspect ratio might change in emulator.
            current_img = cv2.resize(current_img, (master_img.shape[1], master_img.shape[0]))

        # Compute Structural Similarity Index (SSIM) or simple template match
        # For simplicity, we use simple diff
        diff = cv2.absdiff(master_img, current_img)
        non_zero_count = np.count_nonzero(diff)
        total_pixels = master_img.size
        similarity = 1.0 - (non_zero_count / total_pixels)

        passed = similarity >= threshold
        return passed, similarity, "OK" if passed else f"Similarity {similarity:.2f} below threshold {threshold}"

    def detect_glitches(self) -> Tuple[bool, str]:
        """Detect common visual glitches (e.g. garbage tiles)."""
        # Heuristic: Check for high-frequency noise in areas that should be flat.
        # Or look for solid blocks of 'forbidden' colors.
        # This is a placeholder for more advanced logic.
        return False, "Not implemented"

def main():
    parser = argparse.ArgumentParser(description="Visual Verifier for Oracle of Secrets")
    subparsers = parser.add_subparsers(dest="command")

    capture_p = subparsers.add_parser("capture")
    capture_p.add_argument("tag", help="Tag for the master screenshot")

    verify_p = subparsers.add_parser("verify")
    verify_p.add_argument("tag", help="Tag to compare against")
    verify_p.add_argument("--threshold", type=float, default=0.95, help="Similarity threshold (0.0-1.0)")

    args = parser.parse_args()

    verifier = VisualVerifier()

    try:
        if args.command == "capture":
            verifier.save_master(args.tag)
        elif args.command == "verify":
            passed, similarity, msg = verifier.verify(args.tag, args.threshold)
            result = {
                "passed": passed,
                "similarity": similarity,
                "message": msg
            }
            print(json.dumps(result, indent=2))
            if not passed:
                sys.exit(1)
        else:
            parser.print_help()
    except Exception as e:
        print(json.dumps({"error": str(e)}, indent=2))
        sys.exit(1)

if __name__ == "__main__":
    main()
