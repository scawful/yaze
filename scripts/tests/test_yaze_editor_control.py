import importlib.util
import io
import json
from pathlib import Path
import unittest
from unittest.mock import Mock, patch
import urllib.error

spec = importlib.util.spec_from_file_location(
    "yaze_editor_control", Path(__file__).parents[1] / "yaze_editor_control.py")
control = importlib.util.module_from_spec(spec)
spec.loader.exec_module(control)


class EditorControlClientTest(unittest.TestCase):
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
