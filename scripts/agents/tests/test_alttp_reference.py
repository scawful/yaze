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
"""

SRAM = """\
; Current health
CURHP           = $7EF36D
"""


class Fixture(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name) / "usdasm"
        self.maps = Path(self._tmp.name) / "maps"
        self.root.mkdir()
        self.maps.mkdir()
        (self.root / "bank_00.asm").write_text(BANK_00)
        (self.root / "registers.asm").write_text(REGISTERS)
        (self.maps / "symbols_wram.asm").write_text(WRAM)
        (self.maps / "symbols_sram.asm").write_text(SRAM)

    def tearDown(self) -> None:
        self._tmp.cleanup()

    def write_doc(self, body: str) -> Path:
        path = Path(self._tmp.name) / "doc.md"
        path.write_text(textwrap.dedent(body))
        return path


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


class CheckTest(Fixture):
    def check(self, db, body: str, sections=None):
        unverified: list[str] = []
        problems = ar.check_file(db, self.write_doc(body), sections or {}, unverified)
        return problems, unverified

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


if __name__ == "__main__":
    unittest.main()
