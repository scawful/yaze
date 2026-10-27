#!/usr/bin/env python3
"""Host checks for replay generation; these do not qualify editor behavior."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location(
    "lyra_gui_replay", Path(__file__).resolve().parents[1] / "lyra_gui_replay.py")
REPLAY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(REPLAY)


class LyraGuiReplayTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.sidecar = self.root / "input.json"
        self.output = self.root / "episode"
        self.original = {"version": 4, "maps": [
            {"map_id": 0xA0, "tiles16": [[17]], "authoring": {
                "properties": {"message_id": 0x18D},
                "asset_bindings": ["keep"]}},
            {"map_id": 0xA1, "authoring": {
                "properties": {"message_id": 7}}}], "profile": {"keep": 3}}
        self.sidecar.write_text(json.dumps(self.original))

    def test_preparation_preserves_input_and_binds_actual_baseline(self):
        before = self.sidecar.read_bytes()
        REPLAY.prepare(self.sidecar, self.output)
        self.assertEqual(self.sidecar.read_bytes(), before)
        steps = json.loads((self.output / "edit-save.json").read_text())["steps"]
        self.assertEqual(steps[3]["condition"], "value_equals:018D")
        self.assertEqual(steps[4]["text"], "018C")
        targets = [step.get("target") for step in steps]
        self.assertEqual([t for t in targets if t],
                         ["shortcut:Undo", "shortcut:Redo", "shortcut:Save ROM"])

    def test_verify_accepts_only_expected_edit(self):
        REPLAY.prepare(self.sidecar, self.output)
        self.sidecar.write_bytes((self.output / "expected-sidecar.json").read_bytes())
        self.assertEqual(REPLAY.verify(self.sidecar, self.output)["message_id"], 0x18C)

    def test_verify_rejects_no_save(self):
        REPLAY.prepare(self.sidecar, self.output)
        with self.assertRaisesRegex(ValueError, "differs"):
            REPLAY.verify(self.sidecar, self.output)

    def test_verify_rejects_lost_binding_or_other_map_change(self):
        REPLAY.prepare(self.sidecar, self.output)
        expected = json.loads((self.output / "expected-sidecar.json").read_text())
        for mutation in ("binding", "other_map"):
            changed = json.loads(json.dumps(expected))
            if mutation == "binding":
                changed["maps"][0]["authoring"]["asset_bindings"] = []
            else:
                changed["maps"][1]["authoring"]["properties"]["message_id"] = 8
            self.sidecar.write_text(json.dumps(changed))
            with self.assertRaisesRegex(ValueError, "differs"):
                REPLAY.verify(self.sidecar, self.output)

    def check_integer_type_corruption(self, replacement):
        self.original["maps"][0]["authoring"]["properties"]["message_id"] = 0
        self.original["maps"][0]["tiles16"] = [[1]]
        self.original["maps"][1]["authoring"]["properties"]["message_id"] = 1
        self.original["profile"]["keep"] = 1
        self.sidecar.write_text(json.dumps(self.original))
        REPLAY.prepare(self.sidecar, self.output)
        paths = [
            ("maps", 0, "authoring", "properties", "message_id"),
            ("maps", 1, "authoring", "properties", "message_id"),
            ("maps", 0, "tiles16", 0, 0),
            ("profile", "keep"),
        ]
        for path in paths:
            with self.subTest(path=path, replacement=replacement):
                changed = json.loads(
                    (self.output / "expected-sidecar.json").read_text())
                parent = changed
                for key in path[:-1]:
                    parent = parent[key]
                parent[path[-1]] = replacement
                self.sidecar.write_text(json.dumps(changed))
                with self.assertRaisesRegex(ValueError, "differs"):
                    REPLAY.verify(self.sidecar, self.output)

    def test_verify_rejects_boolean_integer_aliases_throughout_document(self):
        self.check_integer_type_corruption(True)

    def test_verify_rejects_float_integer_aliases_throughout_document(self):
        self.check_integer_type_corruption(1.0)

    def test_prepare_refuses_existing_evidence(self):
        REPLAY.prepare(self.sidecar, self.output)
        with self.assertRaises(FileExistsError):
            REPLAY.prepare(self.sidecar, self.output)

    def test_rejects_expected_receipt_mutation(self):
        REPLAY.prepare(self.sidecar, self.output)
        (self.output / "expected-sidecar.json").write_text("{}")
        with self.assertRaisesRegex(ValueError, "receipt changed"):
            REPLAY.verify(self.sidecar, self.output)

    def test_rejects_ambiguous_map_or_invalid_value(self):
        for maps in ([self.original["maps"][0]] * 2,
                     [{"map_id": 0xA0, "authoring": {"properties": {
                         "message_id": True}}}]):
            self.sidecar.write_text(json.dumps({"maps": maps}))
            with self.assertRaises(ValueError):
                REPLAY.prepare(self.sidecar, self.output)
            self.assertFalse(self.output.exists())


if __name__ == "__main__":
    unittest.main()
