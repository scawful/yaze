#!/usr/bin/env python3
"""Control the native editor through its guarded loopback API, without dialogs.

Example:
  yaze_editor_control.py --launch project-open /absolute/project.yaze --room 0x65
  yaze_editor_control.py proposal-load /absolute/proposal.json
  yaze_editor_control.py panel-hide dungeon.object_tile_editor
  yaze_editor_control.py status > /absolute/expected-editor.json
  yaze_editor_control.py project-rom-swap /absolute/candidate.sfc \
    --sha256 <candidate-sha256> --expected-state /absolute/expected-editor.json
  yaze_editor_control.py proposal-offer /absolute/draft.json \
    --out-dir /absolute/proposals

proposal-offer reads the live map (unsaved edits included) for each draft cell,
writes <out-dir>/<id>/proposal.json with those values as `expected`, and offers
it to the running editor. The draft is {id, title, author, why, map_id,
cells: [{x, y, proposed}]}. Nothing is written to the map until the person at
the editor accepts; their decisions land in <out-dir>/<id>/feedback-<id>.json.

--launch starts /Applications/yaze.app only if no Yaze process is running.
An already running app without --editor_control must be restarted deliberately.
ROM swap updates the project descriptor only after exact identity and digest
checks. The client never saves pending edits, discards, edits ROM bytes, or
closes a running app. A rejected swap is never retried with a refreshed identity.
"""
import argparse
import hashlib
import json
import re
from pathlib import Path
import subprocess
import sys
import time
import urllib.error
import urllib.request

PROTOCOL = "yaze-editor-control/v1"
PROPOSAL_SCHEMA = "yaze.overworld_live_proposal.v1"
PROPOSAL_ID = re.compile(r"[A-Za-z0-9._-]{1,64}")


def swap_request(path, sha256, expected_state):
    """Use a caller-retained status receipt, never a freshly probed identity."""
    def digest(value):
        if not isinstance(value, str) or len(value) != 64 or any(
                char not in "0123456789abcdef" for char in value):
            raise ValueError("SHA256 must contain 64 lowercase hexadecimal characters")
        return value

    receipt = json.loads(expected_state.expanduser().read_text())
    if (not isinstance(receipt, dict) or receipt.get("protocol") != PROTOCOL or
            receipt.get("status") != "ok" or not isinstance(receipt.get("state"), dict)):
        raise ValueError("Expected state must be a successful editor status receipt")
    state = receipt["state"]
    expected = {key: state.get(key) for key in
                ("pid", "session_id", "project_path", "rom_path", "rom_sha256")}
    for key in ("pid", "session_id"):
        value = expected[key]
        if type(value) is not int or value < 0 or (key == "pid" and value == 0):
            raise ValueError(f"Expected state has no valid {key}")
    for key in ("project_path", "rom_path"):
        value = expected[key]
        if not isinstance(value, str) or "\0" in value or not Path(value).is_absolute():
            raise ValueError(f"Expected state has no absolute {key}")
    digest(expected["rom_sha256"])
    return {"command": "project.rom.swap", "path": str(path.expanduser().resolve()),
            "candidate_sha256": digest(sha256), "expected": expected}


def proposal_draft(path):
    """Checks a draft before anything is sent; the editor checks it again."""
    draft = json.loads(path.expanduser().read_text())
    if not isinstance(draft, dict):
        raise ValueError("Draft must be a JSON object")
    proposal_id = draft.get("id")
    if (not isinstance(proposal_id, str) or not PROPOSAL_ID.fullmatch(proposal_id)
            or set(proposal_id) == {"."}):
        raise ValueError("Draft id must be 1-64 characters of [A-Za-z0-9._-]")
    if not isinstance(draft.get("title"), str) or not draft["title"]:
        raise ValueError("Draft title is required")
    for field in ("author", "why"):
        if not isinstance(draft.get(field, ""), str):
            raise ValueError(f"Draft {field} must be a string")
    if type(draft.get("map_id")) is not int or draft["map_id"] < 0:
        raise ValueError("Draft map_id must be a nonnegative integer")
    cells = draft.get("cells")
    if not isinstance(cells, list) or not 1 <= len(cells) <= 1024:
        raise ValueError("Draft needs 1-1024 cells")
    seen = set()
    for cell in cells:
        if not isinstance(cell, dict) or any(
                type(cell.get(key)) is not int for key in ("x", "y", "proposed")):
            raise ValueError("Each draft cell needs integer x, y and proposed")
        if not (0 <= cell["x"] < 32 and 0 <= cell["y"] < 32):
            raise ValueError(f"Cell ({cell['x']},{cell['y']}) is outside the 32x32 map")
        if not 0 <= cell["proposed"] <= 0xFFFF:
            raise ValueError(f"Cell ({cell['x']},{cell['y']}) proposed is not a Tile16 ID")
        if (cell["x"], cell["y"]) in seen:
            raise ValueError(f"Draft repeats cell ({cell['x']},{cell['y']})")
        seen.add((cell["x"], cell["y"]))
    return draft


def proposal_packet(draft, read):
    """Packet + expected identity from one overworld.cells.read response.

    Cells whose live tile already equals `proposed` are dropped: accepting them
    would change nothing. Returns (packet, expected, unchanged_cells).
    """
    if read.get("status") != "ok" or read.get("map_id") != draft["map_id"]:
        raise RuntimeError("Live map read failed: " + str(read.get("error", read)))
    live = {(cell["x"], cell["y"]): cell["tile16"] for cell in read["cells"]}
    cells, unchanged = [], []
    for cell in draft["cells"]:
        key = (cell["x"], cell["y"])
        if key not in live:
            raise RuntimeError(f"Live map read did not return cell {key}")
        if live[key] == cell["proposed"]:
            unchanged.append({"x": key[0], "y": key[1]})
            continue
        cells.append({"x": key[0], "y": key[1], "expected": live[key],
                      "proposed": cell["proposed"]})
    if not cells:
        raise RuntimeError("Every draft cell already holds its proposed tile")
    packet = {"schema": PROPOSAL_SCHEMA, "id": draft["id"], "title": draft["title"],
              "author": draft.get("author", ""), "why": draft.get("why", ""),
              "map_id": draft["map_id"], "cells": cells}
    state = read.get("state") or {}
    expected = {key: state.get(key) for key in
                ("pid", "session_id", "project_path", "rom_path", "rom_sha256")}
    if any(value is None for value in expected.values()):
        raise RuntimeError("Editor has no loaded project ROM identity")
    return packet, expected, unchanged


def offer_proposal(port, draft, out_dir):
    """Read live cells, write the packet, and offer it. Never retries."""
    read = command(port, {"command": "overworld.cells.read",
                          "map_id": draft["map_id"],
                          "cells": [{"x": c["x"], "y": c["y"]} for c in draft["cells"]]})
    packet, expected, unchanged = proposal_packet(draft, read)
    folder = out_dir.expanduser().resolve() / draft["id"]
    folder.mkdir(parents=True, exist_ok=True)
    path = folder / "proposal.json"
    data = (json.dumps(packet, indent=2) + "\n").encode()
    path.write_bytes(data)
    result = command(port, {"command": "proposal.offer", "path": str(path),
                            "proposal_sha256": hashlib.sha256(data).hexdigest(),
                            "expected": expected})
    result["packet_path"] = str(path)
    result["unchanged_cells"] = unchanged
    return result


def command(port, body, timeout=120):
    request = urllib.request.Request(
        f"http://127.0.0.1:{port}/api/v1/editor/control",
        data=json.dumps(body).encode(),
        headers={"Content-Type": "application/json", "X-Yaze-Control": "1"},
        method="POST",
    )
    # Ignore HTTP proxy environment variables for the local editor channel.
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
    try:
        response = opener.open(request, timeout=timeout)
    except urllib.error.HTTPError as error:
        response = error
    with response:
        result = json.load(response)
    if result.get("protocol") != PROTOCOL:
        raise RuntimeError("Endpoint is not the guarded Yaze editor interface")
    return result


def connect(port, launch, executable):
    try:
        return command(port, {"command": "status"}, timeout=3)
    except urllib.error.URLError:
        if not launch:
            raise RuntimeError("Editor control is unavailable. Start Yaze with "
                               "--editor_control=true or use --launch after closing it.")
    running = subprocess.run(["pgrep", "-x", "yaze"], capture_output=True,
                             text=True, check=False)
    if running.returncode == 0:
        raise RuntimeError("Yaze is already running without editor control. "
                           "Preserve its work and restart it with --editor_control=true; "
                           "this client will not close it or start a duplicate.")
    if not executable.is_file():
        raise RuntimeError(f"Yaze executable not found: {executable}")
    process = subprocess.Popen(
        [str(executable), "--editor_control=true", f"--api_port={port}"],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        start_new_session=True,
    )
    deadline = time.monotonic() + 30
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError("Yaze exited before the editor API became ready")
        try:
            result = command(port, {"command": "status"}, timeout=3)
            if result.get("status") != "ok" or not result.get("state"):
                time.sleep(0.25)
                continue
            if result["state"].get("pid") != process.pid:
                raise RuntimeError("The responding editor is not the app just launched")
            return result
        except (urllib.error.URLError, TimeoutError):
            time.sleep(0.25)
    raise RuntimeError("Yaze started, but editor control did not become ready; "
                       "the app was left running")


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", type=int, default=8080)
    parser.add_argument("--launch", action="store_true")
    parser.add_argument("--executable", type=Path,
                        default=Path("/Applications/yaze.app/Contents/MacOS/yaze"))
    sub = parser.add_subparsers(dest="action", required=True)
    sub.add_parser("status")
    project = sub.add_parser("project-open")
    project.add_argument("path", type=Path)
    project.add_argument("--room", type=lambda value: int(value, 0))
    swap = sub.add_parser("project-rom-swap")
    swap.add_argument("path", type=Path)
    swap.add_argument("--sha256", required=True)
    swap.add_argument("--expected-state", type=Path, required=True,
                      help="Saved status JSON containing the expected editor identity")
    room = sub.add_parser("dungeon-select")
    room.add_argument("room", type=lambda value: int(value, 0))
    proposal = sub.add_parser("proposal-load")
    proposal.add_argument("path", type=Path)
    offer = sub.add_parser("proposal-offer",
                           help="Offer a live overworld proposal from a draft")
    offer.add_argument("path", type=Path, help="Draft JSON")
    offer.add_argument("--out-dir", type=Path, required=True,
                       help="Folder that receives <id>/proposal.json and feedback-<id>.json")
    panel = sub.add_parser("panel-hide")
    panel.add_argument("panel")
    args = parser.parse_args()
    try:
        swap_body = (swap_request(args.path, args.sha256, args.expected_state)
                     if args.action == "project-rom-swap" else None)
        draft = (proposal_draft(args.path)
                 if args.action == "proposal-offer" else None)
        status = connect(args.port, args.launch, args.executable)
        if status.get("status") != "ok":
            print(json.dumps(status, indent=2))
            return 1
        if swap_body and "project.rom.swap" not in status.get(
                "capabilities", {}).get("commands", []):
            raise RuntimeError("This editor does not support guarded project.rom.swap")
        if args.action == "proposal-offer":
            commands = status.get("capabilities", {}).get("commands", [])
            if not {"overworld.cells.read", "proposal.offer"} <= set(commands):
                raise RuntimeError("This editor does not support live proposals")
            result = offer_proposal(args.port, draft, args.out_dir)
            print(json.dumps(result, indent=2))
            return 0 if result.get("status") == "ok" else 1
        operations = []
        if args.action != "status":
            body = swap_body or {"command": args.action.replace("-", ".")}
            if hasattr(args, "path"):
                body["path"] = str(args.path.expanduser().resolve())
            if args.action == "dungeon-select":
                body["room"] = args.room
            if args.action == "panel-hide":
                body["panel"] = args.panel
            operations.append(body)
        if args.action == "project-open" and args.room is not None:
            operations.append({"command": "dungeon.select", "room": args.room})
        results = []
        for operation in operations:
            status = command(args.port, operation)
            results.append(status)
            if status.get("status") != "ok":
                break
        output = status if len(results) < 2 else {"steps": results, **status}
        print(json.dumps(output, indent=2))
        return 0 if status.get("status") == "ok" else 1
    except (RuntimeError, OSError, ValueError) as error:
        print(json.dumps({"status": "error", "error": str(error)}), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
