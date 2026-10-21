"""Saved-input admission tests; no ROM writer, native process or GUI needed."""

import copy
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import footprint_proposal as planner
import project_proposal as exporter


class ProjectProposalTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.project = self.root / "project.yaze"
        self.receipt = self.root / "native.json"
        self.rom = self.root / "private.sfc"
        self.terrain = self.root / "terrain.json"
        self.body = bytes(0x200000)
        self.rom.write_bytes(self.body)
        self.profile = {
            "graphics_sheets": [58, 59, 60, 61, 71, 72, 62, 91],
            "main_palette": 0, "palette_set": 1, "animated_sheet": 91,
            "tile_count": 4096,
            "tile16_definition_fingerprint": exporter.sha(bytes(32768)),
            "graphics_fingerprint": "a" * 64, "palette_fingerprint": "b" * 64,
            "animation_fingerprint": "c" * 64,
        }
        props = {key: self.profile[key] for key in exporter.VISUAL_FIELDS}
        props.update(sprite_graphics=[-1] * 3, sprite_palettes=[-1] * 3)
        self.document = {
            "schema_version": 3, "profile": self.profile,
            "maps": [{"map_id": i, "tiles16": [[52] * 32 for _ in range(32)],
                      "authoring": {"properties": copy.deepcopy(props),
                                    "asset_bindings": ["d" * 64] * 3}}
                     for i in range(0xA0, 0xC0)],
        }
        self.project.write_text(
            "[files]\nrom_filename=private.sfc\nlyra_terrain_file=terrain.json\n"
            "[rom]\nexpected_hash=" + hashlib.sha1(self.body).hexdigest() + "\n")
        self.refresh()

    def refresh(self):
        self.terrain.write_text(json.dumps(self.document))
        self.observation = {
            "source_rom_sha256": exporter.sha(self.body),
            "terrain_sha256": exporter.sha(self.terrain.read_bytes()),
            "profile": self.profile, "map_count": 32,
            "verified_asset_bindings": [m["authoring"]["asset_bindings"]
                                        for m in self.document["maps"]],
        }
        self.save_receipt()

    def save_receipt(self):
        self.receipt.write_text(json.dumps(self.observation))

    def target(self, **kwargs):
        return json.loads(exporter.export(self.project, self.receipt, 0xB3, **kwargs))

    def footprint(self, target):
        obj = {"schema_version": 1, "units": "tile16", "binding": target["binding"],
               "semantic_id": "fixture.sparse", "review_state": "reviewed",
               "rows": [[0, None], [52, 7]], "anchor": [0, 0],
               "overlap_policy": "reject", "ports": [], "clearance": []}
        obj["content_id"] = planner.content_id(obj)
        path = self.root / "footprint.json"
        path.write_text(json.dumps(obj))
        return path

    def test_actual_snapshot_binding_is_deterministic_and_read_only(self):
        before = {p: p.read_bytes() for p in self.root.iterdir()}
        target = self.target()
        self.assertEqual(target, self.target())
        self.assertEqual(target["rows"], self.document["maps"][19]["tiles16"])
        self.assertEqual(target["binding"]["rom_sha256"], exporter.sha(self.body))
        self.assertEqual(target["binding"]["profile_fingerprint"], planner.digest(self.profile))
        self.assertEqual(before, {p: p.read_bytes() for p in self.root.iterdir()})

    def test_header_included_in_editor_identity_but_not_native_body_hash(self):
        header = bytes([17]) * 512
        self.rom.write_bytes(header + self.body)
        target = self.target()
        self.assertEqual(target["binding"]["rom_sha256"], exporter.sha(header + self.body))

    def test_native_crc32_legacy_and_hash_normalization(self):
        original = hashlib.sha1(self.body).hexdigest()
        text = self.project.read_text()
        for expected in [f"{zlib.crc32(self.body):08x}",
                         f"0x{zlib.crc32(self.body):08X}",
                         f"{exporter.legacy_crc32(self.body):08x}", "0X" + original.upper()]:
            self.project.write_text(text.replace(original, expected))
            self.assertEqual(self.target()["binding"]["rom_sha256"], exporter.sha(self.body))

    def test_stale_native_rom_terrain_and_profile_refuse(self):
        for key, value in [("source_rom_sha256", "e" * 64),
                           ("terrain_sha256", "f" * 64), ("profile", {})]:
            original = self.observation[key]
            self.observation[key] = value
            self.save_receipt()
            with self.assertRaises(ValueError):
                self.target()
            self.observation[key] = original

    def test_independent_definition_and_project_hash_guards(self):
        altered = bytearray(self.body)
        altered[exporter.DEFINITIONS_PC] = 1
        self.rom.write_bytes(altered)
        with self.assertRaisesRegex(ValueError, "expected_hash"):
            self.target()
        self.project.write_text(self.project.read_text().replace(
            hashlib.sha1(self.body).hexdigest(), hashlib.sha1(altered).hexdigest()))
        self.observation["source_rom_sha256"] = exporter.sha(altered)
        self.save_receipt()
        with self.assertRaisesRegex(ValueError, "Tile16 definitions"):
            self.target()

    def test_nonshared_profile_and_stale_native_asset_bindings_refuse(self):
        area = self.document["maps"][19]
        area["authoring"]["properties"]["palette_set"] = 3
        self.refresh()
        with self.assertRaisesRegex(ValueError, "distinct graphics profile"):
            self.target()
        area["authoring"]["properties"]["palette_set"] = 1
        self.refresh()
        self.observation["verified_asset_bindings"] = [["e" * 64] * 3] * 32
        self.save_receipt()
        with self.assertRaisesRegex(ValueError, "asset bindings"):
            self.target()

    def test_sparse_zero_skip_protected_and_exact_bundle(self):
        target = self.target()
        footprint = self.footprint(target)
        bundle = self.target(footprint_path=footprint, placements=[[2, 3]])
        self.assertEqual(bundle["map_id"], 0xB3)
        self.assertEqual(bundle["report"]["changes"], [
            {"x": 2, "y": 3, "before": 52, "after": 0},
            {"x": 3, "y": 4, "before": 52, "after": 7}])
        with self.assertRaisesRegex(ValueError, "protected"):
            self.target(footprint_path=footprint, placements=[[2, 3]], protected=[[2, 3]])

    def test_duplicate_keys_and_suggested_unbound_assemblies_refuse(self):
        self.receipt.write_text('{"map_count":32,"map_count":32}')
        with self.assertRaisesRegex(ValueError, "duplicate JSON key"):
            self.target()
        self.save_receipt()
        path = self.footprint(self.target())
        obj = json.loads(path.read_text())
        obj["review_state"] = "suggested"
        obj["content_id"] = planner.content_id(obj)
        path.write_text(json.dumps(obj))
        with self.assertRaisesRegex(ValueError, "explicitly reviewed"):
            self.target(footprint_path=path, placements=[[2, 3]])

    def test_native_metadata_and_constraint_schema_refusals(self):
        path = self.footprint(self.target())
        original = json.loads(path.read_text())
        for extra in [{"schema_version": True}, {"notes": "unsupported"},
                      {"provenance": {"scale": 0.8}},
                      {"unresolved_behavior": [1]},
                      {"ports": [{"at": [0, 0], "allowed_tile16": [0], "bogus": True}]},
                      {"clearance": [{"at": [0, 0], "allowed_tile16": [0] * 1025}]}]:
            obj = dict(original, **extra)
            obj["content_id"] = planner.content_id(obj)
            path.write_text(json.dumps(obj))
            with self.assertRaises(ValueError):
                self.target(footprint_path=path, placements=[[2, 3]])
        nested = {}
        for _ in range(17):
            nested = {"nested": nested}
        obj = dict(original, provenance=nested)
        obj["content_id"] = planner.content_id(obj)
        path.write_text(json.dumps(obj))
        with self.assertRaisesRegex(ValueError, "nesting"):
            self.target(footprint_path=path, placements=[[2, 3]])

    def test_cli_existing_output_is_never_overwritten(self):
        output = self.root / "keep.json"
        output.write_bytes(b"retain")
        result = subprocess.run([
            sys.executable, "-B", str(Path(exporter.__file__)), str(self.project),
            "--native-receipt", str(self.receipt), "--map", "0xB3", "--out", str(output)],
            capture_output=True, text=True)
        self.assertEqual(result.returncode, 2)
        self.assertEqual(output.read_bytes(), b"retain")


if __name__ == "__main__":
    unittest.main()
