#!/usr/bin/env python3
"""Import legacy coordination-board entries into universe event log.

Default mode is dry-run. Use --apply to append events.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import List


HEADER_RE = re.compile(r"^###\s+(\d{4}-\d{2}-\d{2})\s+([^\s]+)\s+[–-]\s+(.+)$")


@dataclass
class Entry:
    date: str
    agent: str
    title: str
    lines: List[str]


def now_iso() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def derive_project_key(project_root: Path) -> str:
    return str(project_root.resolve()).lstrip("/").replace("/", ":")


def parse_entries(board_path: Path) -> List[Entry]:
    lines = board_path.read_text(encoding="utf-8").splitlines()
    entries: List[Entry] = []
    current: Entry | None = None

    for line in lines:
        m = HEADER_RE.match(line.strip())
        if m:
            if current:
                entries.append(current)
            current = Entry(date=m.group(1), agent=m.group(2), title=m.group(3), lines=[])
            continue
        if current is not None:
            current.lines.append(line.rstrip())

    if current:
        entries.append(current)
    return entries


def infer_status(lines: List[str]) -> str:
    text = "\n".join(lines).upper()
    if "COMPLETE" in text:
        return "complete"
    if "IN_PROGRESS" in text:
        return "active"
    return "open"


def first_note(lines: List[str]) -> str:
    for line in lines:
        s = line.strip()
        if s.startswith("- "):
            return s[2:].strip()
    return ""


def slug(value: str) -> str:
    value = re.sub(r"[^a-zA-Z0-9]+", "_", value.strip())
    return value.strip("_").lower()[:48] or "entry"


def load_existing_task_ids(events_path: Path) -> set[str]:
    out: set[str] = set()
    if not events_path.exists():
        return out
    for raw in events_path.read_text(encoding="utf-8").splitlines():
        raw = raw.strip()
        if not raw:
            continue
        try:
            evt = json.loads(raw)
        except json.JSONDecodeError:
            continue
        tid = evt.get("task_id")
        if tid:
            out.add(tid)
    return out


def build_events(entries: List[Entry], project_key: str, limit: int) -> List[dict]:
    events: List[dict] = []
    for idx, entry in enumerate(entries):
        if limit > 0 and idx >= limit:
            break
        status = infer_status(entry.lines)
        note = first_note(entry.lines)
        task_id = f"import_{entry.date.replace('-', '')}_{idx:04d}_{slug(entry.title)}"
        ts = f"{entry.date}T00:00:00Z"

        events.append(
            {
                "event_id": f"evt_import_{idx:04d}_add",
                "type": "task_added",
                "ts": ts,
                "task_id": task_id,
                "project_key": project_key,
                "agent": entry.agent,
                "to": "",
                "title": entry.title,
                "priority": "B",
                "tags": ["legacy-board", "imported"],
                "note": note,
                "source": "import-coordination-board.py",
            }
        )

        if status in {"active", "complete"}:
            events.append(
                {
                    "event_id": f"evt_import_{idx:04d}_claim",
                    "type": "task_claimed",
                    "ts": ts,
                    "task_id": task_id,
                    "project_key": project_key,
                    "agent": entry.agent,
                    "to": "",
                    "title": "",
                    "priority": "B",
                    "tags": [],
                    "note": "Imported from legacy coordination board",
                    "source": "import-coordination-board.py",
                }
            )
        if status == "complete":
            events.append(
                {
                    "event_id": f"evt_import_{idx:04d}_done",
                    "type": "task_completed",
                    "ts": ts,
                    "task_id": task_id,
                    "project_key": project_key,
                    "agent": entry.agent,
                    "to": "",
                    "title": "",
                    "priority": "B",
                    "tags": [],
                    "note": "Imported completion from legacy coordination board",
                    "source": "import-coordination-board.py",
                }
            )
    return events


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--board",
        default="docs/internal/agents/coordination-board.md",
        help="Path to legacy coordination board markdown",
    )
    parser.add_argument(
        "--universe-dir",
        default=os.path.expanduser("~/.context/agent-universe"),
        help="Universe coordination directory",
    )
    parser.add_argument(
        "--project-key",
        default=None,
        help="Override project key (default: derived from cwd)",
    )
    parser.add_argument(
        "--limit",
        type=int,
        default=0,
        help="Limit imported entries (0 = all)",
    )
    parser.add_argument(
        "--apply",
        action="store_true",
        help="Append generated events to events.jsonl",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Explicitly request dry-run output (default behavior)",
    )
    args = parser.parse_args()

    board_path = Path(args.board)
    if not board_path.exists():
        raise SystemExit(f"board not found: {board_path}")

    project_key = args.project_key or derive_project_key(Path.cwd())
    entries = parse_entries(board_path)
    events = build_events(entries, project_key, args.limit)

    universe_dir = Path(args.universe_dir).expanduser()
    events_path = universe_dir / "events.jsonl"
    state_path = universe_dir / "state.json"

    if not args.apply:
        preview = {
            "mode": "dry-run",
            "generated_at": now_iso(),
            "board_path": str(board_path),
            "project_key": project_key,
            "entries_found": len(entries),
            "events_generated": len(events),
            "sample": events[:5],
        }
        print(json.dumps(preview, indent=2))
        return 0

    universe_dir.mkdir(parents=True, exist_ok=True)
    events_path.touch(exist_ok=True)
    if not state_path.exists():
        state_path.write_text('{"version":1,"updated_at":"","tasks":{}}\n', encoding="utf-8")

    existing_ids = load_existing_task_ids(events_path)
    filtered = [e for e in events if e["task_id"] not in existing_ids]

    with events_path.open("a", encoding="utf-8") as f:
        for event in filtered:
            f.write(json.dumps(event, ensure_ascii=False, separators=(",", ":")) + "\n")

    # Rebuild state via universe-coord script if available.
    script_dir = Path(__file__).resolve().parent
    coord = script_dir / "universe-coord.sh"
    if coord.exists():
        subprocess.run(
            [str(coord), "rebuild-state"],
            check=True,
            capture_output=True,
            text=True,
            env={**os.environ, "UNIVERSE_DIR": str(universe_dir)},
        )

    summary = {
        "mode": "apply",
        "generated_at": now_iso(),
        "board_path": str(board_path),
        "project_key": project_key,
        "entries_found": len(entries),
        "events_generated": len(events),
        "events_appended": len(filtered),
        "events_skipped_existing": len(events) - len(filtered),
        "events_file": str(events_path),
        "state_file": str(state_path),
    }
    print(json.dumps(summary, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
