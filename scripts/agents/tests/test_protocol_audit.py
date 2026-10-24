"""Exercise the maintained router contract in an isolated audit fixture."""

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[3]


class ProtocolAuditTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="yaze-protocol-audit-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        files = [
            "AGENTS.md",
            "docs/internal/agents/personas.md",
            "docs/internal/agents/routing-personas.md",
            "docs/internal/agents/routing-skills-tools.md",
            "docs/internal/agents/universe-coordination-spec.md",
            "docs/internal/agents/operating-rules.md",
            "docs/internal/architecture/refactor-quality-guardrails.md",
            "scripts/agents/protocol-audit.sh",
            "scripts/agents/coord",
            "scripts/agents/coord-heartbeat.sh",
            "scripts/agents/universe-coord.sh",
            "scripts/agents/import-coordination-board.sh",
            "scripts/agents/migrate-coordination-board.sh",
            "scripts/agents/test-universe-coord.sh",
        ]
        for relative in files:
            destination = self.root / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(REPO_ROOT / relative, destination)

    def audit(self):
        return subprocess.run(
            ["/bin/bash", str(self.root / "scripts/agents/protocol-audit.sh")],
            capture_output=True,
            text=True,
            timeout=10,
        )

    def replace(self, relative, before, after):
        path = self.root / relative
        text = path.read_text()
        self.assertIn(before, text)
        path.write_text(text.replace(before, after))

    def assert_rejected(self, message):
        result = self.audit()
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn(message, result.stdout + result.stderr)

    def test_current_compact_router_passes(self):
        result = self.audit()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("PASS: protocol audit", result.stdout)

    def test_missing_operating_rules_route_is_rejected(self):
        self.replace("AGENTS.md", "operating-rules.md", "missing-rules.md")
        self.assert_rejected("AGENTS references operating rules")

    def test_missing_operating_rules_file_is_rejected(self):
        (self.root / "docs/internal/agents/operating-rules.md").unlink()
        self.assert_rejected("missing file: docs/internal/agents/operating-rules.md")

    def test_task_class_drift_is_rejected(self):
        self.replace(
            "docs/internal/agents/routing-skills-tools.md",
            "`ui_ux_editor`",
            "`unknown_class`",
        )
        self.assert_rejected("task classes in routing-skills-tools.md but not")

    def test_unknown_primary_persona_is_rejected(self):
        self.replace(
            "docs/internal/agents/routing-personas.md",
            "`imgui-frontend-engineer`",
            "`unknown-persona`",
        )
        self.assert_rejected("routing persona not declared in personas.md")

    def test_empty_routing_tables_are_rejected(self):
        for name in ("routing-personas.md", "routing-skills-tools.md"):
            (self.root / "docs/internal/agents" / name).write_text("# Empty\n")
        self.assert_rejected("task-class routing tables must not be empty")

    def test_missing_coord_executable_is_rejected(self):
        (self.root / "scripts/agents/coord").chmod(0o644)
        self.assert_rejected("missing executable: scripts/agents/coord")


if __name__ == "__main__":
    unittest.main()
