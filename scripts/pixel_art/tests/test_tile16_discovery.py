"""Synthetic Tile16 discovery repair fixtures; no ROM/exporter/app execution."""
import copy
import gzip
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import tile16_discovery as discovery


def export(rows=None, *, area=0, rom="ROM_A"):
    rows = rows or [[2, 1, 2, 1], [2, 1, 2, 1]]
    profile = {"adapter": discovery.ADAPTER, "palette": [0, 4, 31, 992],
               "background_color": 0, "area_graphics": area}
    records = []
    for tile in sorted({t for row in rows for t in row}):
        words = [tile, tile + 1, tile + 2, tile + 3]
        definition = b"".join(word.to_bytes(2, "little") for word in words)
        records.append({"tile_id": tile, "pixels": [tile] * 256,
                        "transparent_mask": [0, 1] * 128,
                        "definition_fingerprint": discovery.fingerprint(definition),
                        "quadrants": [{"raw_word": word} for word in words],
                        "occurrences": [[x, y] for y, row in enumerate(rows)
                                        for x, value in enumerate(row) if value == tile]})
    return {"schema_version": 1, "adapter": discovery.ADAPTER,
            "transparency_basis": discovery.TRANSPARENCY, "rom_fingerprint": rom,
            "parent_area": area, "requested_map": area,
            "width_tiles": len(rows[0]), "height_tiles": len(rows),
            "profile": profile, "profile_fingerprint": discovery.fingerprint(discovery.canonical(profile)),
            "records": records}


def grid(data):
    rows, _ = discovery.validate_export(data)
    return {"width": data["width_tiles"], "height": data["height_tiles"],
            "rows": rows, "profile_fingerprint": data["profile_fingerprint"],
            "requested_maps": [f'{data["requested_map"]:02X}'], "masked_screens": []}


def candidate(*, kind="strip", rows=None, at=None):
    return {"id": "SR000", "kind": kind, "width": 2, "height": 1,
            "rows": rows or [["0001", "0002"]], "at": at or [["00", 0, 0]],
            "occurrences": 1, "tile32_aligned": 1, "area_count": 1,
            "areas": ["00"], "cells": 2, "score": 2,
            "suggested_label": None, "review_state": "suggested"}


class DiscoveryTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.exports = self.root / "exports"
        self.exports.mkdir()
        self.data = export()
        self.grids = {"00": grid(self.data)}
        self.candidates = [candidate()]

    def write_inputs(self, extras=()):
        (self.root / "grids.json").write_text(json.dumps(self.grids))
        (self.root / "candidates.json").write_text(json.dumps(self.candidates))
        for data in [self.data, *extras]:
            path = self.exports / f'req_{data["requested_map"]:02X}.json'
            path.write_text(json.dumps(data))

    def run_import(self, extras=(), **kwargs):
        self.write_inputs(extras)
        return discovery.import_corpus(self.root / "grids.json", self.root / "candidates.json",
                                       self.exports, **kwargs)

    def test_bad_repeat_anchor_is_reenumerated_and_source_retained(self):
        self.candidates[0]["source_extent"] = [6, 2]
        original = copy.deepcopy(self.candidates[0])
        document, manifest, observations = self.run_import()
        result = document["candidates"][0]
        self.assertEqual(result["at"], [["00", 1, 0], ["00", 1, 1]])
        self.assertEqual(result["source_observation"], original)
        self.assertEqual(result["tile32_aligned"], 0)
        self.assertEqual(result["occurrences"], 2)
        self.assertEqual(manifest["original_replay"]["by_id"], {"SR000": 1})
        self.assertEqual(manifest["corrected_replay"]["bad_occurrences"], 0)
        self.assertFalse(document["agent_writes_enabled"])
        self.assertFalse(result["placement_enabled"])
        self.assertEqual(result["review_state"], "suggested")
        self.assertEqual(result["overlap_policy"], "unapproved")
        self.assertEqual(result["bindings"][0]["profile_fingerprint"], self.data["profile_fingerprint"])
        self.assertEqual(observations["parents"]["00"]["records"], self.data["records"])
        self.assertEqual(self.candidates[0], original)

    def test_every_nonrepeat_occurrence_is_checked(self):
        self.candidates[0] = candidate(kind="stamp", at=[["00", 1, 0], ["00", 0, 1]])
        with self.assertRaisesRegex(ValueError, "Non-repeat source replay failed"):
            self.run_import()

    def test_skip_is_distinct_from_masked_source_and_tile_zero(self):
        g = {"width": 3, "height": 1, "rows": [[0, -1, 2]]}
        self.assertTrue(discovery.matches(g, ((0, None, 2),), 0, 0))
        self.assertFalse(discovery.matches(g, ((None, 0, 2),), 0, 0))
        self.assertFalse(discovery.matches(g, ((0, None, 2),), -1, 0))
        with self.assertRaisesRegex(ValueError, "Invalid Tile16"):
            discovery.parse_pattern({"width": 1, "height": 1, "rows": [["-1"]]})

    def test_cross_parent_mixed_roms_are_rejected(self):
        other = export(area=1, rom="ROM_B")
        self.grids["01"] = grid(other)
        with self.assertRaisesRegex(ValueError, "ROM fingerprint differs across exports"):
            self.run_import([other])

    def test_grid_profile_and_export_profile_tampering_are_rejected(self):
        self.grids["00"]["profile_fingerprint"] = "wrong"
        with self.assertRaisesRegex(ValueError, "Grid/export profile differs"):
            self.run_import()
        self.grids["00"] = grid(self.data)
        self.data["profile"]["palette"][0] = 1
        with self.assertRaisesRegex(ValueError, "Profile fingerprint differs from exported profile"):
            self.run_import()

    def test_grid_dimensions_cells_masks_and_coverage_are_guarded(self):
        changes = [("width", 3, "Grid dimensions differ"),
                   ("rows", [[1, 1, 2, 1], [2, 1, 2, 1]], "Grid/export cell differs"),
                   ("rows", [[-1, 1, 2, 1], [2, 1, 2, 1]], "Grid/export cell differs"),
                   ("requested_maps", ["01"], "Grid/export requested maps differ")]
        for key, value, error in changes:
            with self.subTest(key=key, value=value):
                self.grids["00"] = grid(self.data)
                self.grids["00"][key] = value
                with self.assertRaisesRegex(ValueError, error):
                    self.run_import()

    def test_duplicate_parent_payload_and_definition_changes_are_rejected(self):
        other = copy.deepcopy(self.data)
        other["requested_map"] = 1
        other["records"][0]["labels"] = ["different observation"]
        self.grids["00"]["requested_maps"].append("01")
        with self.assertRaisesRegex(ValueError, "Duplicate export payload differs"):
            self.run_import([other])
        self.data["records"][0]["quadrants"][0]["raw_word"] = 10
        with self.assertRaisesRegex(ValueError, "Definition fingerprint differs"):
            self.run_import()

    def test_same_tile_definition_cannot_change_across_parents(self):
        other = export(area=1)
        record = other["records"][0]
        record["quadrants"][0]["raw_word"] = 10
        words = [q["raw_word"] for q in record["quadrants"]]
        record["definition_fingerprint"] = discovery.fingerprint(
            b"".join(word.to_bytes(2, "little") for word in words))
        self.grids["01"] = grid(other)
        with self.assertRaisesRegex(ValueError, "Definition differs globally"):
            self.run_import([other])

    def test_same_profile_tile_pixels_and_mask_cannot_change_across_parents(self):
        for field, value in (("pixels", 0), ("transparent_mask", 1)):
            with self.subTest(field=field):
                other = export(area=1)
                other["profile"] = copy.deepcopy(self.data["profile"])
                other["profile_fingerprint"] = self.data["profile_fingerprint"]
                other["records"][0][field][0] = value
                self.grids["01"] = grid(other)
                with self.assertRaisesRegex(ValueError, "Appearance differs globally"):
                    self.run_import([other])

    def test_rom_bytes_are_bound_without_running_exporter(self):
        rom = self.root / "source.bin"
        rom.write_bytes(b"synthetic input only")
        self.data["rom_fingerprint"] = discovery.fingerprint(rom.read_bytes())
        _, manifest, _ = self.run_import(rom_path=rom)
        self.assertEqual(manifest["rom"]["bytes"], len(rom.read_bytes()))
        rom.write_bytes(b"different")
        with self.assertRaisesRegex(ValueError, "ROM bytes differ from exports"):
            self.run_import(rom_path=rom)

    def test_parse_and_manifest_hash_share_one_snapshot_during_replacement(self):
        rom = self.root / "source.bin"
        rom.write_bytes(b"synthetic input only")
        self.data["rom_fingerprint"] = discovery.fingerprint(rom.read_bytes())
        self.write_inputs()
        paths = [self.root / "grids.json", self.root / "candidates.json",
                 self.exports / "req_00.json", rom]
        original = {p: p.read_bytes() for p in paths}
        real_snapshot = discovery.snapshot

        def replacing_snapshot(path):
            data, receipt = real_snapshot(path)
            path = Path(path)
            if path in original:
                path.write_bytes(b"DIFFERENT_SOURCE")
            return data, receipt

        with mock.patch.object(discovery, "snapshot", side_effect=replacing_snapshot):
            document, manifest, _ = discovery.import_corpus(
                self.root / "grids.json", self.root / "candidates.json", self.exports,
                rom_path=rom)
        self.assertEqual(document["candidates"][0]["id"], "SR000")
        identities = [manifest["inputs"]["grids"], manifest["inputs"]["candidates"],
                      manifest["inputs"]["exports"][0], manifest["rom"]]
        for path, receipt in zip(paths, identities):
            self.assertEqual(receipt["sha256"], hashlib.sha256(original[path]).hexdigest())
            self.assertNotEqual(receipt["sha256"], hashlib.sha256(path.read_bytes()).hexdigest())

    def test_native_indices_mask_and_yaze_color_expansion(self):
        record = copy.deepcopy(self.data["records"][0])
        record["pixels"] = [0, 1] * 128
        profile = {"palette": [4, 4]}
        image, mask = discovery.native_indexed_tile(record, profile)
        self.assertEqual(image.mode, "P")
        self.assertEqual(image.tobytes(), bytes(record["pixels"]))
        self.assertEqual(image.convert("RGB").getpixel((0, 0)), (33, 0, 0))
        self.assertEqual(image.convert("RGB").getpixel((1, 0)), (33, 0, 0))
        self.assertEqual(mask.tobytes(), bytes([0, 255] * 128))
        preview = discovery.integer_nearest(image, 3)
        for y in range(48):
            for x in range(48):
                self.assertEqual(preview.getpixel((x, y)), image.getpixel((x // 3, y // 3)))
        with self.assertRaisesRegex(ValueError, "integer preview scale"):
            discovery.integer_nearest(image, 1.5)
        for value in range(32):
            self.assertEqual(discovery.bgr555(value)[0], (value << 3) | (value >> 2))

    def test_duplicate_json_keys_are_rejected(self):
        path = self.root / "duplicate.json"
        path.write_text('{"rows":[],"rows":[]}')
        with self.assertRaisesRegex(ValueError, "Duplicate JSON key rows"):
            discovery.read_json(path)

    def test_cli_is_create_only_and_observations_are_lossless(self):
        self.write_inputs()
        out = self.root / "corrected"
        command = [sys.executable, "-B", str(Path(discovery.__file__)),
                   "--grids", str(self.root / "grids.json"),
                   "--candidates", str(self.root / "candidates.json"),
                   "--exports", str(self.exports), "--out", str(out)]
        result = subprocess.run(command, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        observations = json.loads(gzip.decompress((out / "observations.json.gz").read_bytes()))
        self.assertEqual(observations["parents"]["00"]["records"], self.data["records"])
        original = {p.name: p.read_bytes() for p in out.iterdir()}
        again = subprocess.run(command, capture_output=True, text=True)
        self.assertNotEqual(again.returncode, 0)
        self.assertEqual(original, {p.name: p.read_bytes() for p in out.iterdir()})


if __name__ == "__main__":
    unittest.main()
