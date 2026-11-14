import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location(
    "proposal_workbench", Path(__file__).parents[1] / "proposal_workbench.py")
workbench = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(workbench)


class ProposalWorkbenchTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.project = self.root / "Original.yazeproj"
        self.project.mkdir()
        (self.project / "rom").write_bytes(bytes(range(256)))
        (self.project / "source").mkdir()
        (self.project / "source/sky.bin").write_bytes(b"original graphic")
        (self.project / "built.sfc").write_bytes(b"stale build")
        (self.project / "project.yaze.native-save.json").write_text('{"old":true}')
        self.text = (
            "# preserve comments\n[project]\nname=Fixture\n"
            "[files]\nrom_filename=rom\ncode_folder=source\n"
            "[build]\nbuild_script=python3 $YAZE_PROJECT_CODE_FOLDER/build.py\n"
            f"build_target={self.project}/built.sfc\n"
            "[workspace]\nrecent_files=/shared/main.yaze\n"
            "[planning]\nfile=/shared/live-plan.json\n"
            "[resource_labels.room]\n37=Water control\n")
        (self.project / "project.yaze").write_text(self.text)
        self.out = self.root / "proposal"

    def prepare(self, domain="overworld", target=160, entrance=None):
        return workbench.prepare(self.project, self.out, domain, target,
                                 "shore-04", entrance)

    def test_overworld_source_graphics_and_original_survive_edit(self):
        before = workbench.inventory(self.project)
        self.prepare()
        draft = self.out / "Draft.yazeproj"
        (draft / "source/sky.bin").write_bytes(b"revised graphic")
        data = bytearray((draft / "rom").read_bytes())
        data[32:34] = b"ab"
        (draft / "rom").write_bytes(data)
        result = workbench.feedback(self.out)
        self.assertEqual(result["rom_changed_spans"], [{"offset": 32, "length": 2}])
        self.assertEqual({r["path"] for r in result["changed_files"]},
                         {"rom", "source/sky.bin"})
        self.assertEqual(workbench.inventory(self.project), before)
        self.assertEqual((self.out / "baseline-files/source/sky.bin").read_bytes(),
                         b"original graphic")
        self.assertNotEqual((draft / "rom").stat().st_ino,
                            (self.project / "rom").stat().st_ino)

    def test_dungeon_uses_explicit_room_and_entrance(self):
        self.prepare("dungeon", 0x25, 0x31)
        plan = workbench.launch_plan(self.out, Path("/opt/yaze"))
        self.assertIn("--room=37", plan["argv"])
        self.assertIn("--graphics_entrance=49", plan["argv"])
        self.assertIn("--editor=Dungeon", plan["argv"])
        self.assertNotIn("--map=37", plan["argv"])
        self.assertEqual(workbench.feedback(self.out)["domain"], "dungeon")

    def test_dungeon_requires_graphics_context(self):
        with self.assertRaisesRegex(ValueError, "explicit graphics entrance"):
            self.prepare("dungeon", 0x25)
        self.assertFalse(self.out.exists())

    def test_external_editing_target_refused_before_copy(self):
        (self.project / "project.yaze").write_text(
            self.text.replace("rom_filename=rom", "rom_filename=../Main.sfc"))
        with self.assertRaisesRegex(ValueError, "escapes"):
            self.prepare()
        self.assertFalse(self.out.exists())

    def test_external_build_target_refused(self):
        (self.project / "project.yaze").write_text(
            self.text.replace(str(self.project / "built.sfc"), "/shared/Main.sfc"))
        with self.assertRaisesRegex(ValueError, "escapes"):
            self.prepare()

    def test_symlink_cannot_alias_shared_asset(self):
        (self.project / "alias").symlink_to(self.project / "rom")
        with self.assertRaisesRegex(ValueError, "Symlink"):
            self.prepare()

    def test_receipt_and_stale_build_preserved_outside_draft(self):
        self.prepare()
        self.assertFalse((self.out / "Draft.yazeproj/project.yaze.native-save.json").exists())
        self.assertEqual((self.out / "inherited-history/project.yaze.native-save.json").read_text(),
                         '{"old":true}')
        self.assertEqual((self.out / "inherited-history/built.sfc").read_bytes(), b"stale build")

    def test_preserves_labels_and_relocates_only_known_fields(self):
        self.prepare()
        text = (self.out / "Draft.yazeproj/project.yaze").read_text()
        self.assertIn("# preserve comments\n", text)
        self.assertIn("37=Water control", text)
        self.assertIn(f"build_target={self.out}/Draft.yazeproj/built.sfc", text)
        self.assertNotIn("/shared/", text)

    def test_reference_pixels_are_not_imported(self):
        ref = self.root / "temple.png"
        ref.write_bytes(b"unbound illustration")
        packet = workbench.prepare(self.project, self.out, "overworld", 0x90,
                                   "temple", references=[ref])
        self.assertEqual(packet["references"][0]["state"], "unbound-reference-only")
        self.assertEqual(workbench.feedback(self.out)["changed_files"], [])
        self.assertEqual((self.out / "baseline.rom").read_bytes(),
                         (self.out / "Draft.yazeproj/rom").read_bytes())

    def test_modified_baseline_refused(self):
        self.prepare()
        (self.out / "baseline.rom").write_bytes(b"changed")
        with self.assertRaisesRegex(ValueError, "baseline changed"):
            workbench.feedback(self.out)

    def test_modified_source_baseline_refused(self):
        self.prepare()
        (self.out / "baseline-files/source/sky.bin").write_bytes(b"changed")
        with self.assertRaisesRegex(ValueError, "source baseline changed"):
            workbench.feedback(self.out)

    def test_id_cannot_inject_project_fields(self):
        with self.assertRaisesRegex(ValueError, "Proposal ID"):
            workbench.prepare(self.project, self.out, "overworld", 0x90,
                              "draft\n[files]\nrom_filename=/Main.sfc")
        self.assertFalse(self.out.exists())

    def test_modified_draft_path_cannot_escape(self):
        self.prepare()
        path = self.out / "proposal.json"
        data = json.loads(path.read_text())
        data["draft_project"] = "../../Original.yazeproj"
        path.write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError, "escapes"):
            workbench.feedback(self.out)

    def test_output_inside_source_refused(self):
        self.out = self.project / "proposal"
        with self.assertRaisesRegex(ValueError, "outside"):
            self.prepare()

    def test_existing_output_is_preserved(self):
        self.out.mkdir()
        (self.out / "user-note").write_text("keep")
        with self.assertRaisesRegex(ValueError, "new directory"):
            self.prepare()
        self.assertEqual((self.out / "user-note").read_text(), "keep")

    def test_byte_spans_account_for_append_truncate_and_unchanged(self):
        self.assertEqual(workbench.byte_spans(b"abc", b"abc"), [])
        self.assertEqual(workbench.byte_spans(b"abc", b"ab"), [{"offset": 2, "length": 1}])
        self.assertEqual(workbench.byte_spans(b"a", b"abc"), [{"offset": 1, "length": 2}])


if __name__ == "__main__":
    unittest.main()
