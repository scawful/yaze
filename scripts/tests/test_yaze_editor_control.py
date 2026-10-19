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

    def test_rejects_unrelated_service(self):
        opener = Mock()
        opener.open.return_value = io.BytesIO(b'{"status":"ok"}')
        with patch.object(control.urllib.request, "build_opener", return_value=opener):
            with self.assertRaisesRegex(RuntimeError, "not the guarded Yaze"):
                control.command(8080, {"command": "status"})


if __name__ == "__main__":
    unittest.main()
