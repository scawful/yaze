"""Unit tests for scripts/agents/alttp_reference.py using synthetic .asm files.

Run: python3 -m unittest discover -s scripts/agents/tests
"""

from __future__ import annotations

import importlib.util
import sys
import tempfile
import textwrap
import unittest
from pathlib import Path
from unittest import mock

_SCRIPT = Path(__file__).resolve().parents[1] / "alttp_reference.py"
_SPEC = importlib.util.spec_from_file_location("alttp_reference", _SCRIPT)
ar = importlib.util.module_from_spec(_SPEC)
sys.modules["alttp_reference"] = ar
_SPEC.loader.exec_module(ar)

BANK_00 = """\
org $008000

Reset:
#_008000: SEI

; ---------------------------------------------------------

RunModule:
{
.low
#_008010: db ModuleA>>0
#_008011: db ModuleB>>0
}

#Alias:
NMI:
#_008020: SEI

;===========
.local_label
#_008022: RTI

Orphan:
    LDA #$00
#_008030: RTS

#_00FFEA: dw NMI
#_00FFEC: dw Reset
#_00FFEE: dw $FFFF
"""

REGISTERS = """\
INIDISP = $002100
; OAM address
OAMADDL = $002102
"""

WRAM = """\
; Game mode and submode
; See $00:80B5
MODE            = $7E0010
SUBMODE         = $7E0011

; ---------------------------------------------------------
POSX            = $7E0022

; Room ID for underworld
; Copied to $0483
ROOM            = $7E00A0
"""

SRAM = """\
; Current health
CURHP           = $7EF36D
"""

BANK_07 = """\
org $078000

Far:
#_078000: RTS
"""


class Fixture(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name) / "usdasm"
        self.maps = Path(self._tmp.name) / "maps"
        self.root.mkdir()
        self.maps.mkdir()
        (self.root / "bank_00.asm").write_text(BANK_00)
        (self.root / "bank_07.asm").write_text(BANK_07)
        (self.root / "registers.asm").write_text(REGISTERS)
        (self.maps / "symbols_wram.asm").write_text(WRAM)
        (self.maps / "symbols_sram.asm").write_text(SRAM)

    def tearDown(self) -> None:
        self._tmp.cleanup()

    def write_doc(self, body: str) -> Path:
        path = Path(self._tmp.name) / "doc.md"
        path.write_text(textwrap.dedent(body))
        return path

    def check(self, db, body: str, sections=None):
        unverified: list[str] = []
        problems = ar.check_file(db, self.write_doc(body), sections or {}, unverified)
        return problems, unverified


class LoadTest(Fixture):
    def test_labels_skip_braces_local_labels_and_aliases(self) -> None:
        db = ar.Usdasm.load(self.root)
        self.assertEqual(db.labels["Reset"], 0x008000)
        self.assertEqual(db.labels["RunModule"], 0x008010)
        self.assertEqual(db.labels["NMI"], 0x008020)
        self.assertEqual(db.labels["Alias"], 0x008020)
        # A real instruction between a label and its first PC line detaches it.
        self.assertNotIn("Orphan", db.labels)
        self.assertNotIn("local_label", db.labels)

    def test_data_directives_record_targets(self) -> None:
        db = ar.Usdasm.load(self.root)
        self.assertEqual(db.data[0x008010], "ModuleA")
        self.assertEqual(db.data[0x00FFEA], "NMI")
        self.assertEqual(db.data[0x00FFEE], "$FFFF")

    def test_symbol_maps_come_from_symbols_root_with_comment_notes(self) -> None:
        without = ar.Usdasm.load(self.root)
        self.assertFalse(without.has_ram_symbols)
        self.assertEqual(without.symbols["INIDISP"], 0x002100)

        db = ar.Usdasm.load(self.root, self.maps)
        self.assertTrue(db.has_ram_symbols)
        self.assertEqual(db.symbols["CURHP"], 0x7EF36D)
        self.assertEqual(db.symbol_notes["MODE"], "Game mode and submode; See $00:80B5")
        # Consecutive symbols share the block above them.
        self.assertEqual(db.symbol_notes["SUBMODE"], db.symbol_notes["MODE"])
        # A separator-only comment is not a note.
        self.assertNotIn("POSX", db.symbol_notes)

    def test_root_symbol_maps_take_precedence(self) -> None:
        (self.root / "wram.asm").write_text("MODE = $7E0099\n")
        db = ar.Usdasm.load(self.root, self.maps)
        self.assertEqual(db.symbols["MODE"], 0x7E0099)
        self.assertEqual(db.sources["wram"], self.root / "wram.asm")


class RenderTest(Fixture):
    def test_dispatch_table(self) -> None:
        db = ar.Usdasm.load(self.root)
        table = ar.render_dispatch(db, 0x008010, 2, 1)
        self.assertIn("| `0x00` | `ModuleA` |", table)
        self.assertIn("| `0x01` | `ModuleB` |", table)
        with self.assertRaises(SystemExit):
            ar.render_dispatch(db, 0x008010, 3, 1)

    def test_vectors_group_by_handler(self) -> None:
        db = ar.Usdasm.load(self.root)
        with mock.patch.object(ar, "VECTOR_RANGE", (0x00FFEA, 0x00FFEE)):
            table = ar.render_vectors(db)
        self.assertIn("| `NMI` | `$008020` | `$00FFEA` |", table)
        self.assertIn("| `Reset` | `$008000` | `$00FFEC` |", table)
        self.assertIn("| (none: `$FFFF`) | — | `$00FFEE` |", table)

    def test_unknown_routine_or_symbol_is_an_error(self) -> None:
        db = ar.Usdasm.load(self.root, self.maps)
        with mock.patch.object(ar, "ROUTINES", [("Missing", "x")]):
            with self.assertRaises(SystemExit):
                ar.render_routines(db)
        with self.assertRaises(SystemExit):
            ar.render_ram(db, ["NOPE"])

    def test_us_room_note_overrides_incorrect_jp_map_comment(self) -> None:
        db = ar.Usdasm.load(self.root, self.maps)

        table = ar.render_ram(db, ["ROOM"])

        self.assertIn("US code copies $A0 to $048E", table)
        self.assertNotIn("Copied to $0483", table)


class CheckTest(Fixture):
    def test_matching_rows_pass_in_both_orders(self) -> None:
        db = ar.Usdasm.load(self.root, self.maps)
        problems, _ = self.check(db, """\
            | `NMI` | `$008020` | handler |
            | `$2100` | INIDISP | screen |
            | `CURHP` | `$7EF36D` | health |
            """)
        self.assertEqual(problems, [])

    def test_wrong_address_and_unknown_name_fail(self) -> None:
        db = ar.Usdasm.load(self.root, self.maps)
        problems, _ = self.check(db, """\
            | `CURHP` | `$7EF36B` | wrong |
            | `$2102` | OAMADDH | wrong name |
            """)
        self.assertEqual(len(problems), 2)
        self.assertIn("`CURHP` is $7EF36D", problems[0])
        self.assertIn("not a usdasm symbol", problems[1])

    def test_ram_rows_unverified_without_symbol_maps(self) -> None:
        db = ar.Usdasm.load(self.root)
        problems, unverified = self.check(db, """\
            | `CURHP` | `$7EF36D` | health |
            | `NOTREAL` | `$008020` | still checked |
            """)
        self.assertEqual(len(unverified), 1)
        self.assertEqual(len(problems), 1)
        self.assertIn("NOTREAL", problems[0])

    def test_stale_generated_section_fails(self) -> None:
        db = ar.Usdasm.load(self.root)
        sections = {"modules": "| fresh |"}
        body = ("<!-- BEGIN GENERATED: modules -->\n| stale |\n"
                "<!-- END GENERATED: modules -->\n")
        problems, _ = self.check(db, body, sections)
        self.assertEqual(len(problems), 1)
        self.assertIn("stale", problems[0])

    def test_unknown_section_is_a_per_file_problem_in_check(self) -> None:
        db = ar.Usdasm.load(self.root)
        body = ("<!-- BEGIN GENERATED: moduels -->\n| typo |\n"
                "<!-- END GENERATED: moduels -->\n")
        with self.assertRaises(SystemExit):
            ar.apply_sections(body, {})  # render stays strict
        problems, _ = self.check(db, body)
        self.assertEqual(len(problems), 1)
        self.assertIn("unknown generated section `moduels`", problems[0])

    def test_sections_without_sources_are_kept_and_reported(self) -> None:
        db = ar.Usdasm.load(self.root)
        body = ("<!-- BEGIN GENERATED: wram -->\n| kept |\n"
                "<!-- END GENERATED: wram -->\n")
        self.assertEqual(ar.apply_sections(body, {}), body)
        problems, unverified = self.check(db, body)
        self.assertEqual(problems, [])
        self.assertEqual(len(unverified), 1)

    def test_rows_that_are_not_address_tables_are_ignored(self) -> None:
        db = ar.Usdasm.load(self.root)
        problems, unverified = self.check(db, """\
            | Register | Size | Role |
            | A | 8/16-bit | Accumulator |
            | `$00:FFC0` | `0x007FC0` | header |
            ```
            | `CURHP` | `$7EF36B` | fenced rows still count; keep tables out of code blocks |
            ```
            """)
        # Fenced rows are not special-cased: the checker is line-based.
        self.assertEqual(unverified, ["%s:5" % (Path(self._tmp.name) / "doc.md")])
        self.assertEqual(problems, [])


class AddressRulesTest(Fixture):
    def test_four_digit_ram_shorthand_is_unverified_without_maps(self) -> None:
        db = ar.Usdasm.load(self.root)
        problems, unverified = self.check(db, """\
            | `CURHP` | `$F36D` | save-area shorthand |
            | `MODE` | `$0010` | low WRAM shorthand |
            | `TYPO` | `$2100` | register range stays strict |
            """)
        self.assertEqual(len(unverified), 2)
        self.assertEqual(len(problems), 1)
        self.assertIn("TYPO", problems[0])

    def test_four_digit_rows_match_low_bits_six_digit_rows_match_exactly(self) -> None:
        db = ar.Usdasm.load(self.root, self.maps)
        problems, _ = self.check(db, """\
            | `CURHP` | `$F36D` | shorthand ignores the bank |
            | `$2100` | INIDISP | register shorthand |
            | `CURHP` | `$7FF36D` | six digits must match the bank |
            """)
        self.assertEqual(len(problems), 1)
        self.assertIn("doc says $7FF36D", problems[0])

    def test_shorthand_outside_banks_00_and_7e_must_use_six_digits(self) -> None:
        db = ar.Usdasm.load(self.root, self.maps)
        problems, _ = self.check(db, """\
            | `Reset` | `$8000` | bank $00 shorthand is fine |
            | `Far` | `$078000` | six digits in another bank |
            | `Far` | `$8000` | shorthand would alias Reset |
            """)
        self.assertEqual(len(problems), 1)
        self.assertIn("`Far` is $078000", problems[0])
        self.assertIn("six-digit", problems[0])

    def test_conflicting_symbol_redefinition_is_an_error(self) -> None:
        (self.maps / "symbols_sram.asm").write_text(
            "CURHP = $7EF36D\nCURHP = $7EF36D\n")
        ar.Usdasm.load(self.root, self.maps)  # identical repeat is fine
        (self.maps / "symbols_sram.asm").write_text(
            "CURHP = $7EF36D\nCURHP = $7EF36E\n")
        with self.assertRaises(SystemExit):
            ar.Usdasm.load(self.root, self.maps)


class LocateTest(Fixture):
    def test_explicit_or_env_locations_must_be_valid(self) -> None:
        empty = Path(self._tmp.name) / "empty"
        empty.mkdir()
        with mock.patch.dict("os.environ", {}, clear=True):
            self.assertEqual(ar.find_usdasm(str(self.root)), self.root)
            self.assertEqual(ar.find_symbols(str(self.maps)), self.maps)
            with self.assertRaises(SystemExit):
                ar.find_usdasm(str(empty))
            with self.assertRaises(SystemExit):
                ar.find_symbols(str(empty))
        with mock.patch.dict("os.environ", {"YAZE_USDASM_SYMBOLS_DIR": str(empty)}):
            with self.assertRaises(SystemExit):
                ar.find_symbols(None)

    def test_default_symbols_location_is_optional(self) -> None:
        with mock.patch.dict("os.environ", {"HOME": str(Path(self._tmp.name))},
                             clear=True):
            self.assertIsNone(ar.find_symbols(None))


class CliTest(Fixture):
    PATCHES = {
        "MODULE_TABLE": (0x008010, 2, 1),
        "LINK_STATE_TABLE": (0x008010, 2, 1),
        "VECTOR_RANGE": (0x00FFEA, 0x00FFEE),
        "ROUTINES": [("Reset", "entry"), ("NMI", "nmi")],
        "RAM_SYMBOLS": ["MODE"],
        "SRAM_SYMBOLS": ["CURHP"],
    }

    def run_main(self, *argv: str) -> tuple[int, str]:
        import contextlib
        import io
        out = io.StringIO()
        with contextlib.ExitStack() as stack:
            for name, value in self.PATCHES.items():
                stack.enter_context(mock.patch.object(ar, name, value))
            stack.enter_context(contextlib.redirect_stdout(out))
            code = ar.main(list(argv))
        return code, out.getvalue()

    def test_render_then_check_round_trip(self) -> None:
        sections = "".join(
            f"<!-- BEGIN GENERATED: {name} -->\n<!-- END GENERATED: {name} -->\n"
            for name in ar.KNOWN_SECTIONS)
        doc = self.write_doc(sections)
        common = ("--usdasm", str(self.root), "--symbols", str(self.maps))

        code, _ = self.run_main(*common, "render", "--write", str(doc))
        self.assertEqual(code, 0)
        rendered = doc.read_text()
        self.assertIn("| `CURHP` | `$7EF36D` |", rendered)
        self.assertIn("| `0x01` | `ModuleB` |", rendered)

        code, output = self.run_main(*common, "check", str(doc))
        self.assertEqual(code, 0, output)
        self.assertIn("0 problem(s)", output)

        doc.write_text(rendered.replace("`$7EF36D`", "`$7EF36B`"))
        code, output = self.run_main(*common, "check", str(doc))
        self.assertEqual(code, 1)
        self.assertIn("stale", output)


if __name__ == "__main__":
    unittest.main()
