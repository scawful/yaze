import importlib.util
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock, patch
import urllib.error

spec = importlib.util.spec_from_file_location(
    "yaze_editor_control", Path(__file__).parents[1] / "yaze_editor_control.py")
control = importlib.util.module_from_spec(spec)
spec.loader.exec_module(control)


class EditorControlClientTest(unittest.TestCase):
    def swap_receipt(self):
        return {"protocol": control.PROTOCOL, "status": "ok", "state": {
            "pid": 42, "session_id": 7, "project_path": "/private/main.yaze",
            "rom_path": "/private/current.sfc", "rom_sha256": "a" * 64}}

    def test_swap_uses_retained_identity_and_does_not_retry_rejection(self):
        receipt = self.swap_receipt()
        observed = {**receipt, "capabilities": {"commands": ["project.rom.swap"]},
                    "state": {**receipt["state"], "pid": 99}}
        rejected = {**observed, "status": "error", "http_status": 409}
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "expected.json"
            path.write_text(json.dumps(receipt))
            argv = ["control", "project-rom-swap", "/private/next.sfc",
                    "--sha256", "b" * 64, "--expected-state", str(path)]
            with patch.object(control.sys, "argv", argv), \
                 patch.object(control, "connect", return_value=observed), \
                 patch.object(control, "command", return_value=rejected) as command, \
                 patch.object(control.sys, "stdout", io.StringIO()):
                self.assertEqual(control.main(), 1)
            command.assert_called_once()
            body = command.call_args.args[1]
            self.assertEqual(body["command"], "project.rom.swap")
            self.assertEqual(body["expected"], receipt["state"])
            self.assertEqual(body["candidate_sha256"], "b" * 64)

    def test_swap_rejects_malformed_receipt_before_connecting(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "expected.json"
            for field, invalid in (("pid", True), ("session_id", 1.5),
                                   ("rom_path", "relative.sfc"),
                                   ("rom_sha256", "A" * 64)):
                with self.subTest(field=field):
                    receipt = self.swap_receipt()
                    receipt["state"][field] = invalid
                    path.write_text(json.dumps(receipt))
                    argv = ["control", "project-rom-swap", "/private/next.sfc",
                            "--sha256", "b" * 64, "--expected-state", str(path)]
                    with patch.object(control.sys, "argv", argv), \
                         patch.object(control, "connect") as connect, \
                         patch.object(control.sys, "stderr", io.StringIO()):
                        self.assertEqual(control.main(), 1)
                    connect.assert_not_called()

    def test_swap_refuses_editor_without_capability(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "expected.json"
            path.write_text(json.dumps(self.swap_receipt()))
            argv = ["control", "project-rom-swap", "/private/next.sfc",
                    "--sha256", "b" * 64, "--expected-state", str(path)]
            with patch.object(control.sys, "argv", argv), \
                 patch.object(control, "connect", return_value=self.swap_receipt()), \
                 patch.object(control, "command") as command, \
                 patch.object(control.sys, "stderr", io.StringIO()):
                self.assertEqual(control.main(), 1)
            command.assert_not_called()

    def test_sends_explicit_local_header_and_preserves_completed_identity(self):
        body = {"protocol": control.PROTOCOL, "status": "ok",
                "state": {"project_path": "/test.yaze", "pid": 42}}
        opener = Mock()
        opener.open.return_value = io.BytesIO(json.dumps(body).encode())
        with patch.object(control.urllib.request, "build_opener", return_value=opener):
            self.assertEqual(control.command(8080, {"command": "status"}), body)
        request = opener.open.call_args.args[0]
        self.assertEqual(request.full_url, "http://127.0.0.1:8080/api/v1/editor/control")
        self.assertEqual(request.get_header("X-yaze-control"), "1")

    def test_preserves_cancelled_timeout_error(self):
        body = {"protocol": control.PROTOCOL, "status": "error",
                "error": "cancelled without execution"}
        error = urllib.error.HTTPError("http://127.0.0.1", 504, "timeout", {},
                                       io.BytesIO(json.dumps(body).encode()))
        opener = Mock()
        opener.open.side_effect = error
        with patch.object(control.urllib.request, "build_opener", return_value=opener):
            self.assertEqual(control.command(8080, {"command": "status"}), body)

    def test_does_not_launch_duplicate_running_editor(self):
        with patch.object(control, "command", side_effect=urllib.error.URLError("offline")), \
             patch.object(control.subprocess, "run", return_value=Mock(returncode=0)), \
             patch.object(control.subprocess, "Popen") as launch:
            with self.assertRaisesRegex(RuntimeError, "already running"):
                control.connect(8080, True, Path("/Applications/yaze.app/Contents/MacOS/yaze"))
            launch.assert_not_called()

    def offer_status(self, commands=("overworld.cells.read", "proposal.offer")):
        return {**self.swap_receipt(), "capabilities": {"commands": list(commands)}}

    def write_draft(self, directory, **overrides):
        draft = {"id": "ridge-a", "title": "Ridge A", "author": "agent",
                 "why": "test", "map_id": 0x28,
                 "cells": [{"x": 1, "y": 2, "proposed": 0x10},
                           {"x": 3, "y": 4, "proposed": 0x20}], **overrides}
        path = Path(directory) / "draft.json"
        path.write_text(json.dumps(draft))
        return path

    def test_offer_uses_one_read_for_expected_tiles_and_identity(self):
        state = self.swap_receipt()["state"]
        read = {"protocol": control.PROTOCOL, "status": "ok", "map_id": 0x28,
                "state": state,
                "cells": [{"x": 1, "y": 2, "tile16": 0x05},
                          {"x": 3, "y": 4, "tile16": 0x20}]}
        offered = {"protocol": control.PROTOCOL, "status": "ok", "ready": 1}
        with tempfile.TemporaryDirectory() as directory:
            draft = self.write_draft(directory)
            out = Path(directory) / "proposals"
            argv = ["control", "proposal-offer", str(draft), "--out-dir", str(out)]
            with patch.object(control.sys, "argv", argv), \
                 patch.object(control, "connect", return_value=self.offer_status()), \
                 patch.object(control, "command", side_effect=[read, offered]) as command, \
                 patch.object(control.sys, "stdout", io.StringIO()) as stdout:
                self.assertEqual(control.main(), 0)
            reader, offer = (call.args[1] for call in command.call_args_list)
            self.assertEqual(reader, {"command": "overworld.cells.read", "map_id": 0x28,
                                      "cells": [{"x": 1, "y": 2}, {"x": 3, "y": 4}]})
            packet_path = out / "ridge-a" / "proposal.json"
            data = packet_path.read_bytes()
            packet = json.loads(data)
            self.assertEqual(packet["schema"], control.PROPOSAL_SCHEMA)
            # (3,4) already holds its proposed tile, so it is left out.
            self.assertEqual(packet["cells"],
                             [{"x": 1, "y": 2, "expected": 0x05, "proposed": 0x10}])
            self.assertEqual(offer["path"], str(packet_path.resolve()))
            self.assertEqual(offer["proposal_sha256"],
                             control.hashlib.sha256(data).hexdigest())
            self.assertEqual(offer["expected"], state)
            result = json.loads(stdout.getvalue())
            self.assertEqual(result["unchanged_cells"], [{"x": 3, "y": 4}])

    def test_offer_refuses_bad_draft_before_contacting_editor(self):
        bad_drafts = [{"id": ".."}, {"id": "a/b"}, {"title": ""},
                      {"cells": [{"x": 32, "y": 0, "proposed": 1}]},
                      {"cells": [{"x": 0, "y": 0, "proposed": 1},
                                 {"x": 0, "y": 0, "proposed": 2}]}]
        for override in bad_drafts:
            with tempfile.TemporaryDirectory() as directory:
                draft = self.write_draft(directory, **override)
                argv = ["control", "proposal-offer", str(draft),
                        "--out-dir", str(Path(directory) / "out")]
                with patch.object(control.sys, "argv", argv), \
                     patch.object(control, "connect") as connect, \
                     patch.object(control, "command") as command, \
                     patch.object(control.sys, "stderr", io.StringIO()):
                    self.assertEqual(control.main(), 1, override)
                connect.assert_not_called()
                command.assert_not_called()

    def test_offer_writes_nothing_when_read_fails_or_editor_lacks_support(self):
        failed = {"protocol": control.PROTOCOL, "status": "error",
                  "error": "Map must be a ROM overworld map"}
        for status, reads in ((self.offer_status(), [failed]),
                              (self.offer_status(("proposal.offer",)), [])):
            with tempfile.TemporaryDirectory() as directory:
                draft = self.write_draft(directory)
                out = Path(directory) / "out"
                argv = ["control", "proposal-offer", str(draft), "--out-dir", str(out)]
                with patch.object(control.sys, "argv", argv), \
                     patch.object(control, "connect", return_value=status), \
                     patch.object(control, "command", side_effect=reads) as command, \
                     patch.object(control.sys, "stderr", io.StringIO()):
                    self.assertEqual(control.main(), 1)
                self.assertEqual(command.call_count, len(reads))
                self.assertFalse(out.exists())

    def test_rejects_unrelated_service(self):
        opener = Mock()
        opener.open.return_value = io.BytesIO(b'{"status":"ok"}')
        with patch.object(control.urllib.request, "build_opener", return_value=opener):
            with self.assertRaisesRegex(RuntimeError, "not the guarded Yaze"):
                control.command(8080, {"command": "status"})


if __name__ == "__main__":
    unittest.main()
