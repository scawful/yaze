"""Focused ROM-free tests for the Lyra rectangle edit CLI."""

import contextlib
import copy
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "lyra_terrain_edit.py"
SPEC = importlib.util.spec_from_file_location("lyra_terrain_edit", SCRIPT)
terrain = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(terrain)


def fixture(version=1):
    return {"schema": "yaze.lyra-terrain", "schema_version": version,
            "columns": 8, "rows": 4, "map_width": 32, "map_height": 32,
            "tile_pixels": 16, "profile": {"tile_count": 4096,
                                           "binding": "retain verbatim"},
            "maps": [{"map_id": map_id,
                      "tiles16": [[0] * 32 for _ in range(32)],
                      "authoring": {"future": [1, {"value": "kept"}]}}
                     for map_id in range(160, 192)],
            "unknown": {"nested": ["preserved", 4.5]}}


class LyraTerrainEditTest(unittest.TestCase):
    def test_overlapping_copy_uses_snapshot_after_prior_fill(self):
        source = fixture()
        source["maps"][0]["tiles16"][0][:5] = [1, 2, 3, 4, 5]
        source["maps"][0]["tiles16"][1][:5] = [6, 7, 8, 9, 10]
        result = terrain.edit(source, [
            {"op": "fill", "map": 160, "rect": [0, 0, 1, 1], "tile": 11},
            {"op": "copy", "from_map": 160, "rect": [0, 0, 4, 2],
             "to_map": 160, "at": [1, 1]},
            {"op": "copy", "from_map": 160, "rect": [1, 1, 4, 2],
             "to_map": 191, "at": [28, 30]}])
        self.assertEqual(result["maps"][0]["tiles16"][1][1:5], [11, 2, 3, 4])
        self.assertEqual(result["maps"][0]["tiles16"][2][1:5], [6, 7, 8, 9])
        self.assertEqual(result["maps"][-1]["tiles16"][31][28:], [6, 7, 8, 9])
        self.assertEqual(source["maps"][0]["tiles16"][0][0], 1)

    def test_fill_retains_profiles_unknown_fields_and_all_schema_versions(self):
        for version in (1, 2, 3):
            source = fixture(version)
            result = terrain.edit(source, [
                {"op": "fill", "map": 161, "rect": [2, 3, 2, 2], "tile": 707}])
            expected = copy.deepcopy(source)
            for y in (3, 4):
                expected["maps"][1]["tiles16"][y][2:4] = [707, 707]
            self.assertEqual(result, expected)

    def call(self, document, operations, output=None):
        with contextlib.redirect_stdout(io.StringIO()) as stdout:
            with contextlib.redirect_stderr(io.StringIO()) as stderr:
                args = ["--input", str(document), "--operations", str(operations)]
                if output is not None:
                    args += ["--output", str(output)]
                status = terrain.main(args)
        return status, stdout.getvalue(), stderr.getvalue()

    def test_late_invalid_operations_never_create_output(self):
        invalid = [
            {"op": "fill", "map": 160, "rect": [31, 0, 2, 1], "tile": 1},
            {"op": "fill", "map": 159, "rect": [0, 0, 1, 1], "tile": 1},
            {"op": "fill", "map": 160, "rect": [0, 0, 1, 1], "tile": 4096},
            {"op": "fill", "map": 160, "rect": [0, 0, 1, 1], "tile": True},
            {"op": "copy", "from_map": 160, "rect": [0, 0, 2, 2],
             "to_map": 161, "at": [31, 0]}]
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            document, operations, output = [root / name for name in
                                            ("in.json", "ops.json", "out.json")]
            original = json.dumps(fixture())
            document.write_text(original)
            for operation in invalid:
                operations.write_text(json.dumps([
                    {"op": "fill", "map": 160, "rect": [0, 0, 1, 1], "tile": 5},
                    operation]))
                status, _, error = self.call(document, operations, output)
                self.assertEqual(status, 2)
                self.assertIn("Operation 2", error)
                self.assertFalse(output.exists())
                self.assertEqual(document.read_text(), original)

    def test_dry_run_new_output_and_destination_refusal(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            document, operations, output = [root / name for name in
                                            ("in.json", "ops.json", "out.json")]
            document.write_text(json.dumps(fixture()))
            operations.write_text('[{"op":"fill","map":160,"rect":[0,0,2,2],"tile":707}]')
            status, summary, _ = self.call(document, operations)
            self.assertEqual(status, 0)
            self.assertTrue(json.loads(summary)["dry_run"])
            self.assertEqual(json.loads(summary)["changed_cells"], 4)
            self.assertFalse(output.exists())
            self.assertEqual(self.call(document, operations, output)[0], 0)
            self.assertEqual(json.loads(output.read_text())["maps"][0]["tiles16"][0][0], 707)
            saved = output.read_bytes()
            self.assertEqual(self.call(document, operations, output)[0], 2)
            self.assertEqual(output.read_bytes(), saved)
            self.assertEqual(self.call(document, operations, document)[0], 2)
            alias = root / "alias.json"
            alias.symlink_to(document)
            self.assertEqual(self.call(document, operations, alias)[0], 2)

    def test_duplicate_keys_and_malformed_documents_are_refused(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            document, operations, output = [root / name for name in
                                            ("in.json", "ops.json", "out.json")]
            operations.write_text('[]')
            malformed = [dict(fixture(), maps=fixture()["maps"][:-1]),
                         dict(fixture(), schema_version=True),
                         dict(fixture(), columns=8.0)]
            duplicate = fixture()
            duplicate["maps"][1]["map_id"] = 160
            malformed.append(duplicate)
            short_row = fixture()
            short_row["maps"][0]["tiles16"][0].pop()
            malformed.append(short_row)
            for value in malformed:
                document.write_text(json.dumps(value))
                self.assertEqual(self.call(document, operations, output)[0], 2)
                self.assertFalse(output.exists())
            document.write_text('{"schema":"yaze.lyra-terrain","schema":"bad"}')
            self.assertIn("Duplicate JSON key", self.call(document, operations, output)[2])
            document.write_text(json.dumps(fixture()))
            operations.write_text('[{"op":"fill","op":"copy"}]')
            self.assertIn("Duplicate JSON key", self.call(document, operations, output)[2])
            self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
