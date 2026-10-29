#!/usr/bin/env python3
"""Synthetic host checks; they do not execute Project Save As."""
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest

AGENTS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(AGENTS))
import project_saveas_replay as replay


class ProjectSaveAsReplayTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.bundle = self.root / "source"
        self.bundle.mkdir()
        self.output = self.root / "evidence"
        self.destination = self.root / "relocated" / "copy.yaze"
        self.destination.parent.mkdir()
        self.files = {"rom_filename": "rom", "lyra_terrain_file": "sea.json",
                      "lyra_build_request_file": "request.json",
                      "palette_catalog_file": "palette.json",
                      "dungeon_tile_patterns": "patterns.json"}
        for name in (*self.files.values(), "manifest.json", "resources.bin"):
            (self.bundle / name).write_text("synthetic host fixture: " + name)
        (self.bundle / "project.yaze").write_text(
            "[files]\n" + "\n".join(f"{k}={v}" for k, v in self.files.items()))
        replay.prepare(self.bundle, self.destination, self.output)

    def write_synthetic_saved_result(self):
        # This simulates outputs only for verifier host checks, never acceptance.
        relocated = self.destination.with_suffix(".lyra-terrain.json")
        relocated.write_bytes((self.bundle / "sea.json").read_bytes())
        fields = {key: str(self.bundle / value) for key, value in self.files.items()}
        fields["lyra_terrain_file"] = str(relocated)
        self.destination.write_text(
            "[files]\n" + "\n".join(f"{k}={v}" for k, v in fields.items()))

    def test_replay_invokes_existing_save_as_shortcut(self):
        fixture = json.loads((self.output / "save-as.json").read_text())
        self.assertEqual(fixture["steps"][0]["target"], "shortcut:Save As")
        self.assertFalse(self.destination.exists())

    def test_cancel_keeps_source_unchanged(self):
        self.assertTrue(replay.verify(self.output, cancelled=True)["source_unchanged"])

    def test_cancel_rejects_sidecar_or_descriptor_creation(self):
        self.destination.with_suffix(".lyra-terrain.json").write_text("partial")
        with self.assertRaisesRegex(ValueError, "Cancel"):
            replay.verify(self.output, cancelled=True)

    def test_cross_directory_readback_preserves_all_resource_identity(self):
        self.write_synthetic_saved_result()
        self.assertTrue(replay.verify(self.output)["source_unchanged"])

    def test_request_retarget_is_rejected(self):
        self.write_synthetic_saved_result()
        text = self.destination.read_text().replace(
            str(self.bundle / "request.json"), str(self.root / "wrong.json"))
        self.destination.write_text(text)
        with self.assertRaisesRegex(ValueError, "identity mismatch"):
            replay.verify(self.output)

    def test_changed_bin_is_rejected(self):
        self.write_synthetic_saved_result()
        (self.bundle / "resources.bin").write_bytes(b"changed")
        with self.assertRaisesRegex(ValueError, "BIN identity changed"):
            replay.verify(self.output)

    def test_missing_save_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "did not create"):
            replay.verify(self.output)

    def test_new_sidecar_must_match_saved_authoring(self):
        self.write_synthetic_saved_result()
        self.destination.with_suffix(".lyra-terrain.json").write_bytes(b"lost authoring")
        with self.assertRaisesRegex(ValueError, "sidecar identity mismatch"):
            replay.verify(self.output)


if __name__ == "__main__":
    unittest.main()
