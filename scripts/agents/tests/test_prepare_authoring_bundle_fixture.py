import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "prepare_authoring_bundle_fixture.py"
SPEC = importlib.util.spec_from_file_location("bundle_fixture", SCRIPT)
fixture = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(fixture)


class BundleFixtureTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name).resolve()
        self.bundle = self.root / "original.yazeproj"
        self.bundle.mkdir()
        for name, data in {
            "rom": b"test ROM", "manifest.json": b"{}",
            "project.yaze": b"[files]\nrom_filename=rom\ndungeon_tile_patterns=assets/walls.asm\nlyra_terrain_file=assets/lyra.json\n",
            "assets/walls.asm": b"wall patterns", "assets/lyra.json": b"32 maps",
            "assets/palette.bin": b"palette", "tools/yaze": b"app",
        }.items():
            path = self.bundle / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        (self.bundle / "empty").mkdir()
        self.inv = self.root / "inventory.json"
        fixture.write_new(self.inv, fixture.inventory(self.bundle))
        self.pin = fixture.sha256(self.inv)
        self.copy = self.root / "copy.yazeproj"
        self.receipt = self.root / "receipt.json"

    def tearDown(self):
        self.temp.cleanup()

    def prepare(self, **overrides):
        args = dict(bundle=self.bundle, inventory_path=self.inv,
                    expected_hash=self.pin, destination=self.copy,
                    receipt=self.receipt)
        args.update(overrides)
        return fixture.prepare(**args)

    def test_full_copy_preserves_resources_descriptor_and_empty_directories(self):
        before = fixture.inventory(self.bundle)
        result = self.prepare()
        self.assertEqual(result["acceptance"], "UNRUN")
        self.assertFalse(result["native_launched"])
        self.assertEqual(fixture.inventory(self.bundle), before)
        copied = fixture.inventory(self.copy)
        copied["bundle"] = str(self.bundle)
        self.assertEqual(copied, before)
        (self.copy / "rom").write_bytes(b"copy edit")
        self.assertEqual((self.bundle / "rom").read_bytes(), b"test ROM")

    def test_bad_pin_refuses_before_copy(self):
        with self.assertRaisesRegex(ValueError, "SHA-256 mismatch"):
            self.prepare(expected_hash="0" * 64)
        self.assertFalse(self.copy.exists())

    def test_existing_shell_entry_prepares_without_native_launch(self):
        command = ["bash", str(SCRIPT.with_name("run-d6-gui-qualification.sh")),
                   "--prepare-current-bundle", "--bundle", str(self.bundle),
                   "--inventory", str(self.inv), "--inventory-sha256", self.pin,
                   "--destination", str(self.copy), "--receipt", str(self.receipt)]
        result = subprocess.run(command, text=True, capture_output=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)["acceptance"], "UNRUN")
        self.assertEqual((self.copy / "project.yaze").read_bytes(),
                         (self.bundle / "project.yaze").read_bytes())

    def test_changed_source_refuses_before_copy(self):
        (self.bundle / "assets/walls.asm").write_bytes(b"new walls")
        with self.assertRaisesRegex(ValueError, "changed since inventory"):
            self.prepare()
        self.assertFalse(self.copy.exists())

    def test_symlink_and_external_resource_are_refused(self):
        (self.bundle / "link").symlink_to(self.inv)
        with self.assertRaisesRegex(ValueError, "symlink"):
            fixture.inventory(self.bundle)
        (self.bundle / "link").unlink()
        (self.bundle / "project.yaze").write_text("[files]\nrom_filename=../outside\n")
        with self.assertRaisesRegex(ValueError, "nonportable"):
            fixture.inventory(self.bundle)

    def test_existing_destination_and_nested_output_are_refused(self):
        self.copy.mkdir()
        with self.assertRaisesRegex(ValueError, "must not exist"):
            self.prepare()
        with self.assertRaisesRegex(ValueError, "outside"):
            self.prepare(destination=self.bundle / "copy")


if __name__ == "__main__":
    unittest.main()
