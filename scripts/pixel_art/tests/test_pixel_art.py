"""Exact-pixel and export contract tests; no ROM, model, app or network needed."""
import hashlib
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import pixel_art as art


def solid(index, size=8):
    return {"pixels": [[index] * size for _ in range(size)]}


def recipe():
    return {"schema_version": 1, "name": "test", "palette": ["#00000000", "#424242", "#848484", "#C6C6C6", "#FFFFFF"],
            "canvas": {"width": 16, "height": 16, "background": 0},
            "budget": {"max_tiles": 4, "max_unique_tiles": 4},
            "assets": {"a": solid(1), "b": solid(2), "c": solid(3), "d": solid(4)},
            "stamps": [{"asset": name, "at": [x, y]} for name, x, y in
                       (("a", 0, 0), ("b", 8, 0), ("c", 0, 8), ("d", 8, 8))],
            "exports": {"preview_scale": 3, "snes_palette": True}}


def decode_tile(data, bpp):
    """Independent planar decoder, including the separate third-plane layout."""
    rows = []
    for y in range(8):
        low, high = data[2 * y], data[2 * y + 1]
        third = data[16 + y] if bpp == 3 else data[16 + 2 * y]
        fourth = 0 if bpp == 3 else data[17 + 2 * y]
        rows.extend(((low >> shift) & 1) + 2 * ((high >> shift) & 1)
                    + 4 * ((third >> shift) & 1) + 8 * ((fourth >> shift) & 1)
                    for shift in range(7, -1, -1))
    return tuple(rows)


class PixelArtTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def build(self, value=None):
        path = self.root / "recipe.json"
        path.write_text(json.dumps(value or recipe()))
        return art.build_artifacts(path)

    def indexed_reference(self, transparency=0):
        image = Image.frombytes("P", (8, 8), bytes([0, 1, 2, 3] * 16))
        # 1/2 intentionally share RGB, while 0 has transparent RGB equal to 3.
        image.putpalette(bytes([0, 0, 0, 66, 66, 66, 66, 66, 66, 0, 0, 0]))
        image.info["transparency"] = transparency
        path = self.root / "reference.png"
        image.save(path)
        return path

    def sampled_recipe(self, source):
        value = recipe()
        value["canvas"].update(width=8, height=8)
        value["sources"] = {"reference": {"path": source.name, "sha256": art.sha(source.read_bytes())}}
        value["palette"] = ["#00000000", "#424242FF", "#424242FF", "#000000FF"]
        value["assets"] = {"sample": {"sample": {"source": "reference", "crop": [0, 0, 8, 8],
                                                    "indices": {str(i): i for i in range(4)}}}}
        value["stamps"] = [{"asset": "sample", "at": [0, 0]}]
        return value

    def test_known_planar_bytes(self):
        pixels = [0] * 64
        pixels[0], pixels[7] = 1, 8
        data = art.encode_bpp(art.Raster(8, 8, tuple(pixels)), 4)
        expected = bytearray(32)
        expected[0], expected[17] = 0x80, 0x01
        self.assertEqual(data, bytes(expected))
        self.assertEqual(decode_tile(data, 4), tuple(pixels))

    def test_3bpp_third_plane_is_contiguous(self):
        pixels = tuple((x + y * 3) % 8 for y in range(8) for x in range(8))
        data = art.encode_bpp(art.Raster(8, 8, pixels), 3)
        self.assertEqual(len(data), 24)
        self.assertEqual(data[16], 0x0F)
        self.assertEqual(data[17], 0x78)
        self.assertEqual(decode_tile(data, 3), pixels)

    def test_all_4bpp_indices_round_trip(self):
        pixels = tuple((x + 3 * y) % 16 for y in range(8) for x in range(8))
        self.assertEqual(decode_tile(art.encode_bpp(art.Raster(8, 8, pixels)), 4), pixels)

    def test_asymmetric_quadrant_order_and_family_manifest(self):
        files, manifest = self.build()
        for index, value in enumerate((1, 2, 3, 4)):
            self.assertEqual(decode_tile(files["tiles.4bpp"][index * 32:(index + 1) * 32], 4), (value,) * 64)
        self.assertEqual(manifest["families16"], [{"x": 0, "y": 0, "characters": [0, 1, 2, 3]}])
        self.assertEqual(manifest["sheet_columns8"], 2)

    def test_png_indices_palette_and_nearest_preview(self):
        files, _ = self.build()
        atlas = Image.open(io.BytesIO(files["atlas.png"]))
        preview = Image.open(io.BytesIO(files["preview.png"]))
        self.assertEqual((atlas.mode, atlas.size, preview.size), ("P", (16, 16), (48, 48)))
        for y in range(48):
            for x in range(48):
                self.assertEqual(preview.getpixel((x, y)), atlas.getpixel((x // 3, y // 3)))
        self.assertEqual(atlas.convert("RGBA").getpixel((0, 0)), (66, 66, 66, 255))

    def test_indexed_sampling_preserves_duplicate_rgb_and_transparency(self):
        for transparency in (0, bytes([0, 255, 255, 255])):
            with self.subTest(transparency=transparency):
                source = self.indexed_reference(transparency)
                before = source.read_bytes()
                files, manifest = self.build(self.sampled_recipe(source))
                output = Image.open(io.BytesIO(files["atlas.png"]))
                self.assertEqual(output.tobytes(), bytes([0, 1, 2, 3] * 16))
                self.assertEqual(output.convert("RGBA").tobytes(), Image.open(source).convert("RGBA").tobytes())
                self.assertEqual(manifest["sources"][0]["sha256"], art.sha(before))
                self.assertEqual(source.read_bytes(), before)

    def test_rgba_sampling_requires_every_explicit_mapping(self):
        source = self.indexed_reference()
        value = self.sampled_recipe(source)
        sample = value["assets"]["sample"]["sample"]
        del sample["indices"]
        sample["colors"] = {"#00000000": 0, "#424242FF": 1}
        with self.assertRaisesRegex(ValueError, "Unmapped source"):
            self.build(value)
        sample["colors"]["#000000FF"] = 3
        files, _ = self.build(value)
        self.assertEqual(Image.open(io.BytesIO(files["atlas.png"])).convert("RGBA").tobytes(),
                         Image.open(source).convert("RGBA").tobytes())

    def test_reference_pin_and_unmapped_index_guards(self):
        source = self.indexed_reference()
        value = self.sampled_recipe(source)
        value["sources"]["reference"]["sha256"] = "0" * 64
        with self.assertRaisesRegex(ValueError, "SHA256 differs"):
            self.build(value)
        value = self.sampled_recipe(source)
        del value["assets"]["sample"]["sample"]["indices"]["3"]
        with self.assertRaisesRegex(ValueError, "Unmapped source"):
            self.build(value)

    def test_flip_repeat_and_remap_exact_pixels(self):
        value = recipe()
        value["assets"] = {"a": {"pixels": [[1 if x < y else 2 for x in range(8)] for y in range(8)]},
                           "family": {"assemble": {"width": 16, "height": 16, "background": 0,
                                                   "stamps": [{"asset": "a", "at": [0, 0], "repeat": [2, 2],
                                                               "flip_x": True, "remap": {"1": 3}}]}}}
        value["stamps"] = [{"asset": "family", "at": [0, 0], "flip_y": True}]
        raster = art.Compiler(value, self.root).compile()
        for y in range(16):
            for x in range(16):
                original = 1 if 7 - (x % 8) < 7 - (y % 8) else 2
                self.assertEqual(raster.pixels[y * 16 + x], 3 if original == 1 else 2)

    def test_deterministic_artifacts_and_all_hashes(self):
        first, manifest = self.build()
        second, _ = self.build()
        self.assertEqual(first, second)
        for name, entry in manifest["artifacts"].items():
            self.assertEqual(entry, {"bytes": len(first[name]), "sha256": hashlib.sha256(first[name]).hexdigest()})
        art.publish(first, self.root / "out")
        for name, data in first.items():
            self.assertEqual((self.root / "out" / name).read_bytes(), data)

    def test_no_overwrite_or_arbitrary_output_paths(self):
        files, _ = self.build()
        output = self.root / "out"
        output.mkdir()
        sentinel = output / "keep"
        sentinel.write_bytes(b"preserve")
        with self.assertRaisesRegex(ValueError, "already exists"):
            art.publish(files, output)
        self.assertEqual(sentinel.read_bytes(), b"preserve")
        with self.assertRaisesRegex(ValueError, "fixed artifact"):
            art.publish({"../game.sfc": b"x", "manifest.json": b"{}"}, self.root / "fresh")
        self.assertFalse((self.root / "fresh").exists())
        link = self.root / "link"
        link.symlink_to(self.root / "missing")
        with self.assertRaisesRegex(ValueError, "already exists"):
            art.publish(files, link)

    def test_invalid_palette_depth_and_alpha(self):
        for palette in ([], ["#000000"] * 17, ["#QQ0000"], ["#00000080"], ["#000000FF"], ["#00000000", "#00000000"]):
            with self.subTest(palette=palette), self.assertRaises(ValueError):
                value = recipe()
                value["palette"] = palette
                self.build(value)
        value = recipe()
        value["exports"]["bpp"] = 3
        value["palette"] = ["#00000000"] + ["#000000"] * 8
        with self.assertRaisesRegex(ValueError, "3bpp palette exceeds"):
            self.build(value)

    def test_3bpp_exports_exact_cap_and_palette_size(self):
        value = recipe()
        value["exports"]["bpp"] = 3
        files, manifest = self.build(value)
        self.assertIn("tiles.3bpp", files)
        self.assertEqual((len(files["tiles.3bpp"]), len(files["palette.bgr555"]), manifest["bpp"]), (96, 16, 3))

    def test_palette_export_is_exact_or_refuses(self):
        self.assertEqual(art.encode_palette(((255, 0, 0, 255),))[:2], b"\x1f\x00")
        with self.assertRaisesRegex(ValueError, "exact bit-replicated"):
            art.encode_palette(((248, 0, 0, 255),))

    def test_invalid_pixel_indices_are_never_masked(self):
        for index, depth in ((-1, 4), (16, 4), (8, 3), (True, 4)):
            with self.subTest(index=index, depth=depth), self.assertRaises(ValueError):
                art.encode_bpp(art.Raster(8, 8, (index,) * 64), depth)

    def test_alignment_bounds_budget_and_cycles(self):
        bad = []
        value = recipe(); value["canvas"]["width"] = 15; bad.append(value)
        value = recipe(); value["stamps"][0]["at"] = [1, 0]; bad.append(value)
        value = recipe(); value["stamps"][0]["at"] = [16, 0]; bad.append(value)
        value = recipe(); value["budget"]["max_tiles"] = 3; bad.append(value)
        value = recipe(); value["budget"]["max_unique_tiles"] = 3; bad.append(value)
        value = recipe(); value["assets"]["a"] = {"from": "a"}; bad.append(value)
        value = recipe(); value["assets"]["unused"] = {"from": "missing"}; bad.append(value)
        value = recipe(); value["canvas"]["width"] = True; bad.append(value)
        value = recipe(); value["assets"]["a"]["pixels"][1].pop(); bad.append(value)
        value = recipe(); value["assets"]["a"]["remap"] = {"1": -1}; bad.append(value)
        value = recipe(); value["exports"]["preview_scale"] = 1.5; bad.append(value)
        for index, value in enumerate(bad):
            with self.subTest(case=index), self.assertRaises(ValueError):
                self.build(value)

    def test_crop_refuses_padding_and_non_aligned_sample(self):
        source = self.indexed_reference()
        for crop in ([1, 0, 8, 8], [0, 0, 7, 8]):
            value = self.sampled_recipe(source)
            value["assets"]["sample"]["sample"]["crop"] = crop
            with self.subTest(crop=crop), self.assertRaises(ValueError):
                self.build(value)

    def test_inspection_reports_exact_palette_and_partial_edges(self):
        source = self.indexed_reference()
        stats = art.inspect_reference(source)
        self.assertEqual(stats["source_indices"], {0: 16, 1: 16, 2: 16, 3: 16})
        self.assertEqual(stats["rgba_colors"], 3)
        stats = art.inspect_reference(source, [0, 0, 7, 8])
        self.assertEqual(stats["grid8"]["complete_tiles"], 0)
        self.assertEqual(stats["grid8"]["partial_edge_pixels"], [7, 0])

    def test_inspection_separates_sparse_high_indices_from_color_count(self):
        path = self.root / "high-indices.png"
        image = Image.frombytes("P", (8, 8), bytes([0, 255] * 32))
        image.putpalette(bytes(765) + b"\xff\xff\xff")
        image.info["transparency"] = 0
        image.save(path)
        stats = art.inspect_reference(path)
        self.assertTrue(stats["rgba_color_count_fits_16"])
        self.assertTrue(stats["used_index_count_fits_16"])
        self.assertFalse(stats["source_index_range_fits_4bpp"])
        self.assertFalse(stats["source_index_range_fits_3bpp"])

    def test_schema_rejects_unsupported_or_ambiguous_fields(self):
        for patch in ({"schema_version": 2}, {"unknown": True}):
            value = recipe(); value.update(patch)
            with self.subTest(patch=patch), self.assertRaises(ValueError):
                self.build(value)
        source = self.indexed_reference()
        value = self.sampled_recipe(source)
        value["assets"]["sample"]["sample"]["colors"] = {"#00000000": 0}
        with self.assertRaisesRegex(ValueError, "exactly one explicit"):
            self.build(value)

    def test_duplicate_json_keys_rejected(self):
        path = self.root / "duplicate.json"
        path.write_text('{"schema_version":1,"schema_version":2}')
        with self.assertRaisesRegex(ValueError, "Duplicate JSON"):
            art.build_artifacts(path)

    def test_cli_dry_run_and_validation_before_output(self):
        self.build()
        script = Path(art.__file__)
        result = subprocess.run([sys.executable, str(script), "build", str(self.root / "recipe.json"), "--dry-run"],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(json.loads(result.stdout)["dry_run"])
        self.assertEqual(sorted(p.name for p in self.root.iterdir()), ["recipe.json"])
        value = recipe(); value["budget"]["max_tiles"] = 1
        self.build_recipe_file(value)
        output = self.root / "failed-output"
        result = subprocess.run([sys.executable, str(script), "build", str(self.root / "recipe.json"), "--out", str(output)],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 2)
        self.assertFalse(output.exists())

    def build_recipe_file(self, value):
        (self.root / "recipe.json").write_text(json.dumps(value))


if __name__ == "__main__":
    unittest.main()
